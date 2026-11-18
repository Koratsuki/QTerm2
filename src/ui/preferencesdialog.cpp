#include "ui/preferencesdialog.h"

#include "terminal/terminalwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(const QString &fontFamily, int fontSize,
                                     int scrollback, const QString &cursorStyle,
                                     bool blink, const QString &shell, bool dark,
                                     const QString &language, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferencias de QTerm2"));
    setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setSpacing(10);

    m_font = new QFontComboBox(this);
    m_font->setFontFilters(QFontComboBox::MonospacedFonts);
    const QString family = fontFamily.isEmpty()
                               ? QFontDatabase::systemFont(QFontDatabase::FixedFont).family()
                               : fontFamily;
    m_font->setCurrentFont(QFont(family));
    form->addRow(tr("Fuente"), m_font);

    m_size = new QSpinBox(this);
    m_size->setRange(6, 40);
    m_size->setValue(fontSize);
    m_size->setSuffix(tr(" pt"));
    form->addRow(tr("Tamaño"), m_size);

    m_scrollback = new QSpinBox(this);
    m_scrollback->setRange(0, 200000);
    m_scrollback->setSingleStep(1000);
    m_scrollback->setSuffix(tr(" líneas"));
    m_scrollback->setValue(scrollback);
    form->addRow(tr("Historial"), m_scrollback);

    m_cursor = new QComboBox(this);
    m_cursor->addItem(tr("Bloque"), QStringLiteral("block"));
    m_cursor->addItem(tr("I-Beam (barra vertical)"), QStringLiteral("ibeam"));
    m_cursor->addItem(tr("Subrayado"), QStringLiteral("underscore"));
    const QString style = TerminalWidget::cursorStyleName(
        TerminalWidget::cursorStyleFromName(cursorStyle));
    m_cursor->setCurrentIndex(qMax(0, m_cursor->findData(style)));
    form->addRow(tr("Forma del cursor"), m_cursor);

    m_blink = new QCheckBox(tr("Cursor parpadeante"), this);
    m_blink->setChecked(blink);
    m_blink->setToolTip(tr("Uncheck to keep cursor always visible."));
    form->addRow(QString(), m_blink);

    m_shell = new QLineEdit(shell, this);
    auto *browse = new QPushButton(tr("Examinar…"), this);
    auto *shellRow = new QWidget(this);
    auto *shellLayout = new QHBoxLayout(shellRow);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(6);
    shellLayout->addWidget(m_shell, 1);
    shellLayout->addWidget(browse);
    form->addRow(tr("Shell local"), shellRow);

    m_theme = new QComboBox(this);
    m_theme->addItem(tr("Oscuro"), true);
    m_theme->addItem(tr("Claro"), false);
    m_theme->setCurrentIndex(dark ? 0 : 1);
    form->addRow(tr("Tema"), m_theme);

    m_lang = new QComboBox(this);
    m_lang->addItem(QStringLiteral("Español"), QStringLiteral("es"));
    m_lang->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_lang->setCurrentIndex(qMax(0, m_lang->findData(language.isEmpty()
                                                        ? QStringLiteral("en")
                                                        : language)));
    form->addRow(tr("Language"), m_lang);

    layout->addLayout(form);

    auto *hint = new QLabel(tr("Los cambios se aplican a todas las terminales abiertas. "
                               "Las sesiones SSH activas no se reconectan."),
                           this);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("Hint"));
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *save = buttons->addButton(tr("Guardar"), QDialogButtonBox::AcceptRole);
    save->setDefault(true);
    layout->addWidget(buttons);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Seleccionar shell"), QStringLiteral("/bin"));
        if (!file.isEmpty())
            m_shell->setText(file);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &QDialog::accept);
}

QString PreferencesDialog::fontFamily() const
{
    return m_font->currentFont().family();
}

int PreferencesDialog::fontSize() const
{
    return m_size->value();
}

int PreferencesDialog::scrollback() const
{
    return m_scrollback->value();
}

QString PreferencesDialog::cursorStyle() const
{
    return m_cursor->currentData().toString();
}

bool PreferencesDialog::blink() const
{
    return m_blink->isChecked();
}

QString PreferencesDialog::shell() const
{
    return m_shell->text().trimmed();
}

bool PreferencesDialog::darkTheme() const
{
    return m_theme->currentData().toBool();
}

QString PreferencesDialog::language() const
{
    return m_lang->currentData().toString();
}
