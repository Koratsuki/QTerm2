#pragma once

#include "storage/database.h"
#include "storage/snippetrepository.h"

namespace qterm2 {

class SqliteSnippetRepository final : public SnippetRepository
{
public:
    explicit SqliteSnippetRepository(Database *database);

    bool loadAll(QVector<Snippet> &snippets,
                 QString *errorMessage = nullptr) override;
    bool upsert(Snippet &snippet, QString *errorMessage = nullptr) override;
    bool remove(const QString &id, QString *errorMessage = nullptr) override;

private:
    Database *m_database = nullptr;
};

} // namespace qterm2
