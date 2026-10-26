#pragma once

#include <QDateTime>
#include <QString>

#include "domain/session.h"

namespace qterm2 {

/**
 * A reusable command template.
 *
 * Snippets are plain text with optional placeholders; they are never executed
 * by QTerm2 itself. The UI expands the placeholders for the active session and
 * types the result into the terminal, so the user still reviews and confirms.
 */
struct Snippet
{
    QString id;
    QString name;
    QString command;
    QString description;
    QDateTime createdAt;
    QDateTime updatedAt;
};

/**
 * Expands `${host}`, `${user}`, `${port}`, `${name}` and `${group}`.
 *
 * `$$` produces a literal dollar sign. Unknown placeholders are left untouched
 * so a typo is visible instead of silently becoming an empty string.
 */
QString expandSnippet(const QString &text, const Session &session);

} // namespace qterm2
