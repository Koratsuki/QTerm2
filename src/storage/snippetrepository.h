#pragma once

#include "domain/snippet.h"

#include <QString>
#include <QVector>

namespace qterm2 {

/** Storage-neutral snippet catalog. */
class SnippetRepository
{
public:
    virtual ~SnippetRepository() = default;

    virtual bool loadAll(QVector<Snippet> &snippets,
                         QString *errorMessage = nullptr) = 0;
    virtual bool upsert(Snippet &snippet, QString *errorMessage = nullptr) = 0;
    virtual bool remove(const QString &id, QString *errorMessage = nullptr) = 0;
};

} // namespace qterm2
