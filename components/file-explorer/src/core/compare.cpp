// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements compare.h.

#include "core/compare.h"

#include <QFile>
#include <QSet>

#include <fts.h>
#include <sys/stat.h>

#include <algorithm>

namespace fe {

namespace {

/// FAT and exFAT keep modification times to two seconds; a copy there and back is still the same file.
constexpr qint64 TIME_SLACK = 2;

} // namespace

QHash<QString, ItemFacts> walkFolder(const QString &folder, const std::atomic_bool &cancel)
{
    QHash<QString, ItemFacts> items;
    QByteArray root = QFile::encodeName(folder);
    char *argv[] = {root.data(), nullptr};
    FTS *tree = ::fts_open(argv, FTS_PHYSICAL | FTS_XDEV | FTS_NOCHDIR, nullptr);
    if (!tree) {
        return items;
    }
    const qsizetype prefix = root.endsWith('/') ? root.size() : root.size() + 1;
    while (FTSENT *node = ::fts_read(tree)) {
        if (cancel.load(std::memory_order_relaxed)) {
            break;
        }
        if (node->fts_level == 0 || node->fts_info == FTS_DP || node->fts_info == FTS_DNR || node->fts_info == FTS_ERR
            || node->fts_info == FTS_NS) {
            continue;
        }
        const QString path = QFile::decodeName(QByteArray(node->fts_path + prefix));
        const struct stat *info = node->fts_statp;
        items.insert(path, {node->fts_info == FTS_D || node->fts_info == FTS_DC, qint64(info->st_size), qint64(info->st_mtim.tv_sec)});
    }
    ::fts_close(tree);
    return items;
}

QList<CompareRow> compareFolders(const QHash<QString, ItemFacts> &left, const QHash<QString, ItemFacts> &right,
                                 const std::function<bool(const QString &)> &sameContents)
{
    QList<CompareRow> rows;
    QSet<QString> paths(left.keyBegin(), left.keyEnd());
    paths.unite(QSet<QString>(right.keyBegin(), right.keyEnd()));
    rows.reserve(paths.size());
    for (const QString &path : std::as_const(paths)) {
        const auto l = left.constFind(path);
        const auto r = right.constFind(path);
        if (r == right.constEnd()) {
            rows.append({path, CompareStatus::OnlyLeft});
            continue;
        }
        if (l == left.constEnd()) {
            rows.append({path, CompareStatus::OnlyRight});
            continue;
        }
        if (l->folder != r->folder) {
            rows.append({path, CompareStatus::Different});
            continue;
        }
        if (l->folder) {
            rows.append({path, CompareStatus::Same});
            continue;
        }
        const int newer = l->modified > r->modified + TIME_SLACK ? 1 : r->modified > l->modified + TIME_SLACK ? -1 : 0;
        bool same = l->size == r->size;
        if (same) {
            same = sameContents ? sameContents(path) : newer == 0;
        }
        rows.append({path, same ? CompareStatus::Same : CompareStatus::Different, same ? 0 : newer});
    }
    std::sort(rows.begin(), rows.end(), [](const CompareRow &a, const CompareRow &b) { return a.path < b.path; });
    return rows;
}

QString compareSummary(const QList<CompareRow> &rows)
{
    qsizetype counts[4] = {};
    for (const CompareRow &row : rows) {
        ++counts[int(row.status)];
    }
    return QStringLiteral("%1 only left, %2 only right, %3 different, %4 the same").arg(counts[0]).arg(counts[1]).arg(counts[2]).arg(counts[3]);
}

} // namespace fe
