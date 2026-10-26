#include "sessions/snippetstore.h"

#include <algorithm>

namespace qterm2 {

SnippetStore::SnippetStore(SnippetRepository *repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
{
}

bool SnippetStore::load()
{
    m_snippets.clear();
    if (!m_repository) {
        setError(QStringLiteral("No hay un repositorio de snippets configurado."));
        return false;
    }
    QString error;
    if (!m_repository->loadAll(m_snippets, &error)) {
        setError(error);
        emit changed();
        return false;
    }
    m_lastError.clear();
    emit changed();
    return true;
}

void SnippetStore::setError(const QString &message)
{
    m_lastError = message;
    emit errorOccurred(message);
}

int SnippetStore::indexOf(const QString &id) const
{
    for (int i = 0; i < m_snippets.size(); ++i) {
        if (m_snippets.at(i).id == id)
            return i;
    }
    return -1;
}

Snippet SnippetStore::snippetById(const QString &id) const
{
    const int index = indexOf(id);
    return index >= 0 ? m_snippets.at(index) : Snippet();
}

bool SnippetStore::saveSnippet(Snippet &snippet)
{
    if (!m_repository) {
        setError(QStringLiteral("No hay un repositorio de snippets configurado."));
        return false;
    }
    QString error;
    if (!m_repository->upsert(snippet, &error)) {
        setError(error);
        return false;
    }

    const int existing = indexOf(snippet.id);
    if (existing >= 0)
        m_snippets[existing] = snippet;
    else
        m_snippets.append(snippet);

    std::sort(m_snippets.begin(), m_snippets.end(),
              [](const Snippet &a, const Snippet &b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });

    m_lastError.clear();
    emit changed();
    return true;
}

bool SnippetStore::removeSnippet(const QString &id)
{
    if (!m_repository) {
        setError(QStringLiteral("No hay un repositorio de snippets configurado."));
        return false;
    }
    QString error;
    if (!m_repository->remove(id, &error)) {
        setError(error);
        return false;
    }
    const int index = indexOf(id);
    if (index >= 0)
        m_snippets.removeAt(index);
    m_lastError.clear();
    emit changed();
    return true;
}

} // namespace qterm2
