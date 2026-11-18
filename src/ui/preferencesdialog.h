#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QSpinBox;

/**
 * Terminal and UI preferences.
 *
 * The dialog owns no state outside itself: it is filled from plain values and
 * read back through the accessors, so MainWindow remains the only place that
 * knows how preferences are persisted.
 */
class PreferencesDialog : public QDialog
{
public:
    PreferencesDialog(const QString &fontFamily, int fontSize, int scrollback,
                      const QString &cursorStyle, bool blink,
                      const QString &shell, bool dark, const QString &language,
                      QWidget *parent = nullptr);

    QString fontFamily() const;
    int fontSize() const;
    int scrollback() const;
    QString cursorStyle() const;
    bool blink() const;
    QString shell() const;
    bool darkTheme() const;
    QString language() const;

private:
    QFontComboBox *m_font = nullptr;
    QSpinBox *m_size = nullptr;
    QSpinBox *m_scrollback = nullptr;
    QComboBox *m_cursor = nullptr;
    QCheckBox *m_blink = nullptr;
    QLineEdit *m_shell = nullptr;
    QComboBox *m_theme = nullptr;
    QComboBox *m_lang = nullptr;
};
