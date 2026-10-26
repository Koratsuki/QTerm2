#pragma once

#include <QDialog>

#include "domain/session.h"
#include "domain/snippet.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace qterm2 {
class SnippetStore;
}

/**
 * Snippet manager and launcher.
 *
 * The dialog edits the catalog in place and, when the user presses Insertar,
 * returns the command expanded for the active session. QTerm2 never executes a
 * snippet on its own: MainWindow types it into the terminal so the user can
 * review or cancel it.
 */
class SnippetDialog : public QDialog
{
    Q_OBJECT
public:
    SnippetDialog(qterm2::SnippetStore *store, const Session &context,
                  QWidget *parent = nullptr);

    /** Expanded command chosen with Insertar, or an empty string. */
    QString commandToSend() const { return m_commandToSend; }

private:
    void reload(const QString &selectId = QString());
    void loadIntoEditor(const qterm2::Snippet &snippet);
    void clearEditor();
    void saveCurrent();
    void deleteCurrent();
    void insertCurrent();
    void updatePreview();
    void applyFilter(const QString &text);
    bool isNew() const { return m_currentId.isEmpty(); }

    qterm2::SnippetStore *m_store = nullptr;
    Session m_context;
    QString m_currentId;
    QString m_commandToSend;

    QLineEdit *m_filter = nullptr;
    QListWidget *m_list = nullptr;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_command = nullptr;
    QPlainTextEdit *m_description = nullptr;
    QLabel *m_preview = nullptr;
    QPushButton *m_insert = nullptr;
    QPushButton *m_save = nullptr;
    QPushButton *m_delete = nullptr;
    QPushButton *m_new = nullptr;
};
