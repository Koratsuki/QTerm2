#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "domain/snippet.h"
#include "storage/snippetrepository.h"

namespace qterm2 {

/** In-memory snippet catalog backed by SnippetRepository. */
class SnippetStore : public QObject
{
    Q_OBJECT
public:
    explicit SnippetStore(SnippetRepository *repository, QObject *parent = nullptr);

    const QVector<Snippet> &snippets() const { return m_snippets; }
    bool load();
    QString lastError() const { return m_lastError; }

    int indexOf(const QString &id) const;
    Snippet snippetById(const QString &id) const;

    bool saveSnippet(Snippet &snippet);
    bool removeSnippet(const QString &id);

signals:
    void changed();
    void errorOccurred(const QString &message);

private:
    void setError(const QString &message);

    SnippetRepository *m_repository = nullptr;
    QVector<Snippet> m_snippets;
    QString m_lastError;
};

} // namespace qterm2
