#include "terminal/terminalwidget.h"
#include "terminal/terminalemulator.h"


#include "terminal/ptyterminalsession.h"
#include "terminal/terminalsession.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QUrl>
#include <QWheelEvent>

#include <QtMath>

#include <algorithm>

// --------------------------------------------------------------- utilities

namespace {

QFont pickDefaultFont()
{
    QStringList preferred = {
        QStringLiteral("JetBrains Mono NL"),
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("Hack Nerd Font Mono"),
        QStringLiteral("Hack Nerd Font"),
        QStringLiteral("FiraCode Nerd Font Mono"),
        QStringLiteral("Fira Code"),
        QStringLiteral("Cascadia Code"),
        QStringLiteral("Source Code Pro"),
        QStringLiteral("IBM Plex Mono"),
        QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("Liberation Mono"),
        QStringLiteral("Consolas"),
        QStringLiteral("Monospace"),
    };
    const QStringList installed = QFontDatabase::families();
    QFont f;
    for (const QString &name : preferred) {
        if (installed.contains(name, Qt::CaseInsensitive)) {
            f = QFont(name);
            break;
        }
    }
    if (f.family().isEmpty())
        f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setStyleHint(QFont::TypeWriter, QFont::PreferQuality);
    f.setFixedPitch(true);
    f.setPointSize(11);
    f.setHintingPreference(QFont::PreferFullHinting);
    return f;
}

} // namespace

// ------------------------------------------------------------- construction

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent)
    , m_font(pickDefaultFont())
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setMouseTracking(true);
    setFont(m_font);

    m_boldFont = m_font;
    m_boldFont.setBold(true);
    m_italicFont = m_font;
    m_italicFont.setItalic(true);
    m_boldItalicFont = m_font;
    m_boldItalicFont.setBold(true);
    m_boldItalicFont.setItalic(true);

    m_colors = Theme::TerminalColors::forMode(Theme::Mode::Dark);

    QFontMetricsF fm(m_font);
    m_cellW = (std::max)(1, qRound(fm.horizontalAdvance(QLatin1Char('M'))));
    m_cellH = (std::max)(1, qCeil(fm.height()));
    m_ascent = qRound(fm.ascent());

    m_blinkTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_blinkTimer, &QTimer::timeout, this, [this] {
        m_cursorBlinkOn = !m_cursorBlinkOn;
        update();
    });

    m_emulator = new TerminalEmulator(this);
    connect(m_emulator, &TerminalEmulator::updated, this, [this] {
        if (m_blinkTimer.isActive())
            m_blinkTimer.start();
        update();
    });
    connect(m_emulator, &TerminalEmulator::screenReset, this, [this] {
        m_scrollOffset = 0;
        m_hasSelection = false;
        m_searchAbsLine = -1;
        m_searchCol = -1;
        emit selectionChanged(false);
    });
    connect(m_emulator, &TerminalEmulator::titleChanged, this,
            [this](const QString &title) {
                m_title = title;
                emit titleChanged(title);
            });
    connect(m_emulator, &TerminalEmulator::bellRang,
            this, &TerminalWidget::bellRang);
    connect(m_emulator, &TerminalEmulator::workingDirectoryChanged,
            this, &TerminalWidget::workingDirectoryChanged);
    connect(m_emulator, &TerminalEmulator::response, this,
            [this](const QByteArray &data) {
                if (m_transport)
                    m_transport->write(data);
            });
    connect(m_emulator, &TerminalEmulator::cursorStyleRequested, this,
            [this](TerminalEmulator::CursorStyle style, bool blink, bool isDefault) {
                // The running application may choose the shape, but DECSCUSR 0
                // returns to the style configured by the user.  The user's
                // "blinking cursor" preference remains the master switch.
                m_cursorStyle = isDefault ? m_preferredCursorStyle : style;
                m_cursorBlinkRequested = blink;
                applyCursorBlink();
            });

    setCursorBlinking(true);
    setTransport(new qterm2::PtyTerminalSession(this));
}

TerminalWidget::~TerminalWidget()
{
    if (m_transport)
        m_transport->terminate();
}

void TerminalWidget::setTransport(qterm2::TerminalSession *transport)
{
    if (m_transport == transport)
        return;
    if (m_transport) {
        m_transport->disconnect(this);
        m_transport->disconnect(m_emulator);
        m_transport->terminate();
        m_transport->deleteLater();
    }
    m_transport = transport;
    if (!m_transport)
        return;
    m_transport->setParent(this);
    connect(m_transport, &qterm2::TerminalSession::dataReceived,
            m_emulator, &TerminalEmulator::feed);
    connect(m_transport, &qterm2::TerminalSession::finished, this, [this] {
        emit terminalFinished();
        update();
    });
}

// ---------------------------------------------------------- process control

bool TerminalWidget::isRunning() const
{
    return m_transport && m_transport->isRunning();
}

bool TerminalWidget::startShell(const QString &shell, const QString &workDir)
{
    if (!m_transport)
        return false;
    return m_transport->startShell(shell, m_emulator->cols(),
                                   m_emulator->rows(), workDir);
}

bool TerminalWidget::startProgram(const QString &program, const QStringList &args,
                                  const QString &workDir,
                                  const QProcessEnvironment &extraEnv)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QStringList keys = extraEnv.keys();
    for (const QString &k : keys)
        env.insert(k, extraEnv.value(k));
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    env.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    env.insert(QStringLiteral("TERM_PROGRAM"), QStringLiteral("QTerm2"));
    env.insert(QStringLiteral("TERM_PROGRAM_VERSION"), QStringLiteral("1.0"));
    const QString locale = env.value(QStringLiteral("LANG"));
    if (!locale.contains(QStringLiteral("UTF-8"), Qt::CaseInsensitive)
        && !locale.contains(QStringLiteral("utf8"), Qt::CaseInsensitive))
        env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));

    if (!m_transport)
        return false;
    return m_transport->start(program, args, m_emulator->cols(),
                              m_emulator->rows(), workDir, env);
}

void TerminalWidget::sendText(const QString &text)
{
    if (!m_transport)
        return;
    const QByteArray data = text.toUtf8();
    m_transport->write(data);
    emit userInput(data);
}

// --------------------------------------------------------------- geometry

void TerminalWidget::updateTerminalSize()
{
    QFontMetricsF fm(m_font);
    m_cellW = (std::max)(1, qRound(fm.horizontalAdvance(QLatin1Char('M'))));
    m_cellH = (std::max)(1, qCeil(fm.height()));
    m_ascent = qRound(fm.ascent());
    const int rows = qMax(2, height() / m_cellH);
    const int cols = qMax(2, width() / m_cellW);
    m_emulator->resize(rows, cols);
    m_scrollOffset = qBound(0, m_scrollOffset, historyRows());
    if (m_transport)
        m_transport->resize(cols, rows);
}

// -------------------------------------------------------------------- theming

QColor TerminalWidget::colorFor(int c, bool foreground) const
{
    if (c < 0)
        return foreground ? m_colors.foreground : m_colors.background;

    static const int ramp[6] = { 0, 0x5f, 0x87, 0xaf, 0xd7, 0xff };
    if (c & 0x1000000)
        return QColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
    if (c < 16)
        return m_colors.palette[c];
    if (c < 232) {
        const int v = c - 16;
        return QColor(ramp[v / 36], ramp[(v / 6) % 6], ramp[v % 6]);
    }
    const int g = 8 + (c - 232) * 10;
    return QColor(g, g, g);
}

// --------------------------------------------------------------- view helpers

int TerminalWidget::historyRows() const
{
    return m_emulator->historyRows();
}

int TerminalWidget::totalLines() const
{
    return m_emulator->totalLines();
}

QString TerminalWidget::plainLine(int absoluteLine) const
{
    return m_emulator->plainLine(absoluteLine);
}

const TermLine *TerminalWidget::lineForView(int row) const
{
    const int abs = absoluteLineForView(row);
    if (abs < 0)
        return nullptr;
    const int hist = historyRows();
    if (abs < hist)
        return &m_emulator->historyBuffer().at(abs);
    const int idx = abs - hist;
    const TermBuffer &buffer = m_emulator->activeBuffer();
    if (idx >= 0 && idx < buffer.size())
        return &buffer.at(idx);
    return nullptr;
}

int TerminalWidget::absoluteLineForView(int row) const
{
    const int abs = historyRows() - m_scrollOffset + row;
    return (abs >= 0 && abs < totalLines()) ? abs : -1;
}

// -------------------------------------------------------------------- painting

void TerminalWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), m_colors.background);
    p.setFont(m_font);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const bool focused = hasFocus();
    const QColor selectionBg = m_colors.selection;
    const QColor selectionFg = (m_colors.selection.lightnessF() > 0.55)
                                   ? QColor(0x10, 0x12, 0x16)
                                   : Qt::white;

    QVector<QPair<int, int>> ranges;
    for (int row = 0; row < m_emulator->rows(); ++row) {
        const int abs = absoluteLineForView(row);
        const TermLine *line = lineForView(row);
        const int y = row * m_cellH;
        if (!line)
            continue;

        if (!m_searchText.isEmpty()) {
            searchRangesForLine(abs, ranges);
        } else {
            ranges.clear();
        }

        for (int col = 0; col < m_emulator->cols() && col < line->size(); ++col) {
            const TermCell &cell = line->at(col);
            if (cell.ch.unicode() == 0)
                continue; // trailing half of a wide glyph

            int fg = cell.fg;
            int bg = cell.bg;
            quint8 attrs = cell.attrs;
            if (attrs & AttrInverse)
                std::swap(fg, bg);

            const bool selected = isSelected(abs, col);

            bool inSearch = false;
            for (const auto &r : ranges) {
                if (col >= r.first && col <= r.second) {
                    inSearch = true;
                    break;
                }
            }

            const int cells = cell.wide ? 2 : 1;
            const QRect cellRect(col * m_cellW, y, m_cellW * cells, m_cellH);

            if (selected)
                p.fillRect(cellRect, selectionBg);
            else if (bg != -1)
                p.fillRect(cellRect, colorFor(bg, false));
            else if (inSearch)
                p.fillRect(cellRect, QColor(255, 200, 40, 55));

            if ((attrs & AttrInvisible) || cell.ch == QLatin1Char(' '))
                continue;
            if (cell.ch.unicode() < 32)
                continue;

            QColor fgColor = selected ? selectionFg : colorFor(fg, true);
            if (attrs & AttrFaint)
                fgColor.setAlpha(150);

            QFont font = m_font;
            if ((attrs & AttrBold) && (attrs & AttrItalic))
                font = m_boldItalicFont;
            else if (attrs & AttrBold)
                font = m_boldFont;
            else if (attrs & AttrItalic)
                font = m_italicFont;
            p.setFont(font);
            p.setPen(fgColor);
            p.drawText(col * m_cellW, y + m_ascent, QString(cell.ch));
            p.setFont(m_font);

            if (attrs & AttrUnderline)
                p.fillRect(QRect(col * m_cellW, y + m_cellH - 2, m_cellW * cells, 1), fgColor);
            if (attrs & AttrStrike)
                p.fillRect(QRect(col * m_cellW, y + m_cellH / 2, m_cellW * cells, 1), fgColor);
        }
    }

    // ---- cursor ----------------------------------------------------------
    if (m_emulator->cursorVisible() && m_scrollOffset == 0 && m_cursorBlinkOn) {
        const int cx = qBound(0, m_emulator->cursorX(), m_emulator->cols() - 1);
        const int cy = qBound(0, m_emulator->cursorY(), m_emulator->rows() - 1);
        const int x = cx * m_cellW;
        const int y = cy * m_cellH;
        const QRect r(x, y, m_cellW, m_cellH);
        const QColor cursorColor = m_colors.cursor;

        switch (m_cursorStyle) {
        case TerminalEmulator::CursorStyle::Block:
            if (focused) {
                p.fillRect(r, cursorColor);
                const TermLine *line = lineForView(cy);
                if (line && cx < line->size()) {
                    const QChar ch = line->at(cx).ch;
                    if (ch.unicode() != u' ' && ch.unicode() != 0) {
                        p.setPen(m_colors.background);
                        p.setFont(m_font);
                        p.drawText(x, y + m_ascent, QString(ch));
                    }
                }
            } else {
                p.setPen(QPen(cursorColor, 1));
                p.drawRect(r.adjusted(0, 0, -1, -1));
            }
            break;

        case TerminalEmulator::CursorStyle::IBeam: {
            // U+258F LEFT ONE EIGHTH BLOCK: a solid bar that fills the cell
            // height and sits on the left edge of the cell.  ASCII "|" is
            // centred inside its advance width, which makes the cursor look
            // like it is floating in the middle of the character.
            p.save();
            p.setClipRect(r);
            // The glyph is only ~2px wide; subpixel antialiasing would split
            // it into coloured fringes instead of a solid bar.
            QFont cursorFont = m_font;
            cursorFont.setStyleStrategy(QFont::NoAntialias);
            p.setFont(cursorFont);
            p.setPen(cursorColor);
            p.drawText(QPointF(x, y + m_ascent), QString(QChar(0x258F)));
            p.restore();
            break;
        }

        case TerminalEmulator::CursorStyle::Underscore: {
            const int thick = focused ? 3 : 1;
            p.fillRect(QRect(x, y + m_cellH - thick, m_cellW, thick), cursorColor);
            break;
        }
        }
    }
}

// ----------------------------------------------------------------------- input

QString TerminalWidget::screenText() const
{
    return m_emulator->screenText();
}

void TerminalWidget::clearScrollback()
{
    m_emulator->clearHistory();
    m_scrollOffset = 0;
    update();
}

void TerminalWidget::resetTerminal()
{
    m_emulator->reset();
    m_scrollOffset = 0;
    m_hasSelection = false;
    m_searchAbsLine = -1;
    m_searchCol = -1;
    update();
}

