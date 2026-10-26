#include "ui/theme.h"

#include <QApplication>
#include <QSettings>
#include <QVector>

#include <algorithm>
#include <cstring>

namespace Theme {

namespace {
Mode g_mode = Mode::Dark;
}

Mode currentMode()
{
    return g_mode;
}

void setCurrentMode(Mode mode)
{
    g_mode = mode;
    if (QApplication *app = qobject_cast<QApplication *>(QCoreApplication::instance())) {
        app->setPalette(applicationPalette(mode));
        app->setStyleSheet(styleSheet(mode));
    }
}

void apply(QApplication &app)
{
    app.setPalette(applicationPalette(g_mode));
    app.setStyleSheet(styleSheet(g_mode));
}

TerminalColors TerminalColors::dark()
{
    TerminalColors c;
    c.background = QColor(0x10, 0x12, 0x16);
    c.foreground = QColor(0xd8, 0xde, 0xe9);
    c.cursor     = QColor(0x63, 0xb3, 0xed);
    c.selection  = QColor(0x2f, 0x5d, 0x8a);
    const QColor p[16] = {
        QColor(0x1c, 0x1f, 0x26), QColor(0xe0, 0x6c, 0x75),
        QColor(0x98, 0xc3, 0x79), QColor(0xe5, 0xc0, 0x7b),
        QColor(0x61, 0xaf, 0xef), QColor(0xc6, 0x78, 0xdd),
        QColor(0x56, 0xb6, 0xc2), QColor(0xab, 0xb2, 0xbf),
        QColor(0x5c, 0x63, 0x70), QColor(0xf2, 0x7f, 0x88),
        QColor(0xa9, 0xd6, 0x8a), QColor(0xf0, 0xd0, 0x8c),
        QColor(0x7c, 0xc4, 0xff), QColor(0xd7, 0x99, 0xef),
        QColor(0x6f, 0xd0, 0xdc), QColor(0xff, 0xff, 0xff),
    };
    for (int i = 0; i < 16; ++i)
        c.palette[i] = p[i];
    return c;
}

TerminalColors TerminalColors::light()
{
    TerminalColors c;
    c.background = QColor(0xfa, 0xfa, 0xfa);
    c.foreground = QColor(0x24, 0x29, 0x2e);
    c.cursor     = QColor(0x0b, 0x63, 0xd3);
    c.selection  = QColor(0xb9, 0xd9, 0xff);
    const QColor p[16] = {
        QColor(0x24, 0x29, 0x2e), QColor(0xc0, 0x39, 0x39),
        QColor(0x2e, 0x7d, 0x32), QColor(0xa8, 0x6a, 0x00),
        QColor(0x15, 0x65, 0xc0), QColor(0x8e, 0x24, 0xaa),
        QColor(0x00, 0x83, 0x8a), QColor(0x6a, 0x73, 0x7d),
        QColor(0x5c, 0x63, 0x70), QColor(0xd3, 0x3b, 0x3b),
        QColor(0x39, 0x92, 0x3f), QColor(0xbf, 0x7c, 0x00),
        QColor(0x1a, 0x73, 0xd8), QColor(0xa4, 0x3a, 0xc0),
        QColor(0x00, 0x96, 0x9d), QColor(0x1c, 0x21, 0x26),
    };
    for (int i = 0; i < 16; ++i)
        c.palette[i] = p[i];
    return c;
}

TerminalColors TerminalColors::forMode(Mode mode)
{
    return mode == Mode::Dark ? dark() : light();
}

QPalette applicationPalette(Mode mode)
{
    const bool dark = (mode == Mode::Dark);
    QPalette pal;
    if (dark) {
        pal.setColor(QPalette::Window,          QColor(0x16, 0x18, 0x1d));
        pal.setColor(QPalette::WindowText,      QColor(0xd8, 0xde, 0xe9));
        pal.setColor(QPalette::Base,            QColor(0x13, 0x15, 0x1a));
        pal.setColor(QPalette::AlternateBase,   QColor(0x1a, 0x1d, 0x23));
        pal.setColor(QPalette::ToolTipBase,     QColor(0x1a, 0x1d, 0x23));
        pal.setColor(QPalette::ToolTipText,     QColor(0xd8, 0xde, 0xe9));
        pal.setColor(QPalette::Text,            QColor(0xd8, 0xde, 0xe9));
        pal.setColor(QPalette::Button,          QColor(0x1a, 0x1d, 0x23));
        pal.setColor(QPalette::ButtonText,      QColor(0xd8, 0xde, 0xe9));
        pal.setColor(QPalette::BrightText,      Qt::white);
        pal.setColor(QPalette::Link,            QColor(0x63, 0xb3, 0xed));
        pal.setColor(QPalette::Highlight,       QColor(0x2f, 0x5d, 0x8a));
        pal.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
        pal.setColor(QPalette::PlaceholderText, QColor(0x6b, 0x72, 0x80));
    } else {
        pal.setColor(QPalette::Window,          QColor(0xf2, 0xf3, 0xf5));
        pal.setColor(QPalette::WindowText,      QColor(0x24, 0x29, 0x2e));
        pal.setColor(QPalette::Base,            QColor(0xff, 0xff, 0xff));
        pal.setColor(QPalette::AlternateBase,   QColor(0xf3, 0xf4, 0xf6));
        pal.setColor(QPalette::ToolTipBase,     QColor(0xff, 0xff, 0xff));
        pal.setColor(QPalette::ToolTipText,     QColor(0x24, 0x29, 0x2e));
        pal.setColor(QPalette::Text,            QColor(0x24, 0x29, 0x2e));
        pal.setColor(QPalette::Button,          QColor(0xf6, 0xf7, 0xf9));
        pal.setColor(QPalette::ButtonText,      QColor(0x24, 0x29, 0x2e));
        pal.setColor(QPalette::BrightText,      Qt::black);
        pal.setColor(QPalette::Link,            QColor(0x0b, 0x63, 0xd3));
        pal.setColor(QPalette::Highlight,       QColor(0x0b, 0x63, 0xd3));
        pal.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
        pal.setColor(QPalette::PlaceholderText, QColor(0x7a, 0x82, 0x8d));
    }
    return pal;
}

QString styleSheet(Mode mode)
{
    const bool dark = (mode == Mode::Dark);
    struct Token { const char *key; QString value; };
    const QVector<Token> tokens = {
        { "BG",        dark ? "#16181d" : "#f2f3f5" },
        { "BGDEEP",    dark ? "#0f1116" : "#e7e9ed" },
        { "PANEL",     dark ? "#1b1e24" : "#ffffff" },
        { "PANELALT",  dark ? "#20242b" : "#f6f7f9" },
        { "BORDER",    dark ? "#2a2f38" : "#d6d9df" },
        { "TEXT",      dark ? "#d8dee9" : "#24292e" },
        { "TEXTDIM",   dark ? "#8b93a1" : "#6a737d" },
        { "ACCENT",    dark ? "#3b82f6" : "#0b63d3" },
        { "ACCENTDIM", dark ? "#1f3a5f" : "#cfe2ff" },
        { "HOVER",     dark ? "#262b34" : "#eef1f5" },
        { "ACCENTHI",  dark ? "#4b93ff" : "#0a58bd" },
    };

    QString s = QStringLiteral(R"(
QMainWindow, QDialog { background: @BG; color: @TEXT; }
QWidget { color: @TEXT; }
QToolTip { background: @PANEL; color: @TEXT; border: 1px solid @BORDER; padding: 4px 6px; }
QMenuBar { background: @BG; color: @TEXT; border: none; padding: 2px; }
QMenuBar::item { background: transparent; padding: 5px 10px; border-radius: 4px; }
QMenuBar::item:selected { background: @HOVER; }
QMenu { background: @PANEL; color: @TEXT; border: 1px solid @BORDER; padding: 4px; }
QMenu::item { padding: 6px 26px 6px 24px; border-radius: 4px; }
QMenu::item:selected { background: @ACCENT; color: #ffffff; }
QMenu::separator { height: 1px; background: @BORDER; margin: 5px 8px; }

QToolBar { background: @BGDEEP; border: none; border-bottom: 1px solid @BORDER; spacing: 4px; padding: 5px 8px; }
QToolBar::separator { width: 1px; background: @BORDER; margin: 4px 6px; }
QToolButton { background: transparent; border: none; border-radius: 6px; padding: 5px 9px; color: @TEXT; }
QToolButton:hover { background: @HOVER; }
QToolButton:pressed { background: @ACCENTDIM; }
QToolButton:checked { background: @ACCENTDIM; color: @ACCENTHI; }

QStatusBar { background: @BGDEEP; color: @TEXTDIM; border-top: 1px solid @BORDER; }
QStatusBar::item { border: none; }

QLineEdit { background: @PANELALT; color: @TEXT; border: 1px solid @BORDER; border-radius: 6px;
            padding: 6px 9px; selection-background-color: @ACCENT; selection-color: #ffffff; }
QLineEdit:focus { border: 1px solid @ACCENT; }
QPlainTextEdit, QTextEdit { background: @PANELALT; color: @TEXT; border: 1px solid @BORDER; border-radius: 6px; }
QComboBox { background: @PANELALT; color: @TEXT; border: 1px solid @BORDER; border-radius: 6px; padding: 6px 9px; min-height: 18px; }
QComboBox:hover { border: 1px solid @TEXTDIM; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView { background: @PANEL; color: @TEXT; border: 1px solid @BORDER;
    selection-background-color: @ACCENT; selection-color: #ffffff; outline: none; padding: 3px; }
QSpinBox { background: @PANELALT; color: @TEXT; border: 1px solid @BORDER; border-radius: 6px; padding: 5px 7px; }
QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid @TEXTDIM; border-radius: 4px; background: @PANELALT; }
QCheckBox::indicator:checked { background: @ACCENT; border-color: @ACCENT; }
QGroupBox { border: 1px solid @BORDER; border-radius: 8px; margin-top: 14px; padding: 12px 10px 10px 10px; font-weight: 600; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: @TEXTDIM; }

QPushButton { background: @PANEL; color: @TEXT; border: 1px solid @BORDER; border-radius: 6px; padding: 7px 14px; }
QPushButton:hover { background: @HOVER; }
QPushButton:pressed { background: @ACCENTDIM; }
QPushButton:default { background: @ACCENT; color: #ffffff; border-color: @ACCENT; }
QPushButton:default:hover { background: @ACCENTHI; }
QPushButton:disabled { color: @TEXTDIM; }

QTabWidget::pane { border: none; background: @BGDEEP; }
QTabBar { background: @BGDEEP; qproperty-drawBase: 0; }
QTabBar::tab { background: transparent; color: @TEXTDIM; border: none; padding: 8px 14px 7px 14px;
               margin: 4px 2px 0 2px; border-top-left-radius: 7px; border-top-right-radius: 7px;
               min-width: 80px; }
QTabBar::tab:hover { background: @HOVER; color: @TEXT; }
QTabBar::tab:selected { background: @BG; color: @TEXT; border-top: 2px solid @ACCENT; }

QSplitter::handle { background: @BORDER; }
QSplitter::handle:horizontal { width: 1px; }
QSplitter::handle:vertical { height: 1px; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: @BORDER; min-height: 24px; border-radius: 4px; }
QScrollBar::handle:vertical:hover { background: @TEXTDIM; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: @BORDER; min-width: 24px; border-radius: 4px; }

QTreeWidget { background: transparent; color: @TEXT; border: none; outline: none; }
QTreeWidget::item { padding: 6px 4px; border-radius: 5px; }
QTreeWidget::item:hover { background: @HOVER; }
QTreeWidget::item:selected { background: @ACCENT; color: #ffffff; }
QTreeWidget::branch { background: transparent; }

QHeaderView::section { background: @PANEL; color: @TEXTDIM; border: none; padding: 4px; }

QLabel#SidebarTitle { color: @TEXTDIM; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
QLabel#Hint { color: @TEXTDIM; }
QLabel#Link { color: @ACCENTHI; }
QLabel#Brand { color: @TEXT; font-weight: 700; }
QWidget#Sidebar { background: @PANEL; border-right: 1px solid @BORDER; }
QFrame#SearchBar { background: @BGDEEP; border-bottom: 1px solid @BORDER; }
QWidget#Welcome { background: @BG; }
QToolButton#SidebarAction { background: @PANELALT; border: 1px solid @BORDER; border-radius: 6px;
                            padding: 5px 8px; color: @TEXT; }
QToolButton#SidebarAction:hover { background: @HOVER; }
QToolButton#SidebarPrimary { background: @ACCENT; border: 1px solid @ACCENT; border-radius: 6px;
                             padding: 5px 8px; color: #ffffff; font-weight: 600; }
QToolButton#SidebarPrimary:hover { background: @ACCENTHI; border-color: @ACCENTHI; }
QFrame#TopBar { background: @BGDEEP; border-bottom: 1px solid @BORDER; }
QWidget#TerminalHost { background: @BGDEEP; }
QLabel#PaneBadge { background: @ACCENTDIM; color: @TEXTDIM; border-radius: 4px; padding: 2px 6px; font-size: 10px; }
QDialog QLabel { color: @TEXT; }
)");

    // Placeholders are written as "@KEY" in the template above. Replace the
    // longest keys first so "@ACCENT" cannot damage "@ACCENTDIM"/"@ACCENTHI".
    QVector<Token> ordered = tokens;
    std::sort(ordered.begin(), ordered.end(), [](const Token &a, const Token &b) {
        return std::strlen(a.key) > std::strlen(b.key);
    });
    for (const Token &t : ordered)
        s.replace(QLatin1Char('@') + QLatin1String(t.key), t.value);
    return s;
}

} // namespace Theme
