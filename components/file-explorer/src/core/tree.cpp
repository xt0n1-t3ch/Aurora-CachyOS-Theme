// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements tree.h.

#include "core/tree.h"

namespace fe {

namespace {

void appendRows(QList<TreeEntry> &rows, const QString &folder, int depth, const QSet<QString> &expanded,
                const std::function<QStringList(const QString &)> &subfolders)
{
    if (!expanded.contains(folder)) {
        return;
    }
    for (const QString &name : subfolders(folder)) {
        const QString path = folder + u'/' + name;
        const bool open = expanded.contains(path) && depth < TREE_DEPTH_LIMIT;
        rows.append({path, name, depth, open});
        if (open) {
            appendRows(rows, path, depth + 1, expanded, subfolders);
        }
    }
}

} // namespace

QList<TreeEntry> expandedRows(const QString &root, const QSet<QString> &expanded,
                              const std::function<QStringList(const QString &)> &subfolders)
{
    QList<TreeEntry> rows;
    appendRows(rows, root, 1, expanded, subfolders);
    return rows;
}

} // namespace fe
