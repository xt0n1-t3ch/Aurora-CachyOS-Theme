// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Comparing two folders, as Compare Folders shows them: what is only on one side, what differs and what is the same,
// through every subfolder.

#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include <atomic>
#include <functional>

namespace fe {

/// What a walk knows of one item, by its path relative to the folder compared.
struct ItemFacts {
    bool folder = false;
    qint64 size = 0;
    /// Seconds since the epoch.
    qint64 modified = 0;
};

enum class CompareStatus {
    OnlyLeft,
    OnlyRight,
    /// Both sides hold it but it is not the same: another size, a newer side, or a file on one side and a folder on the
    /// other.
    Different,
    Same,
};

struct CompareRow {
    /// The path relative to both folders, with "/" between folders.
    QString path;
    CompareStatus status = CompareStatus::Same;
    /// For a Different row: which side was changed last (1 left, -1 right, 0 the same time or not files).
    int newer = 0;
};

/// Every item below folder, by its path relative to it; links are items of their own and are not followed, and other
/// filesystems mounted inside are left out. Stops early, with what it has, when cancel becomes true.
[[nodiscard]] QHash<QString, ItemFacts> walkFolder(const QString &folder, const std::atomic_bool &cancel);

/// The rows of a comparison, sorted by path. Two files are the same when their sizes match and their modification
/// times are within two seconds (FAT keeps times to two seconds), or, when sameContents is given, when it says their
/// contents match. Folders are the same when both sides hold them; what is inside them has rows of its own.
[[nodiscard]] QList<CompareRow> compareFolders(const QHash<QString, ItemFacts> &left, const QHash<QString, ItemFacts> &right,
                                               const std::function<bool(const QString &)> &sameContents = {});

/// The line under a comparison: "3 only left, 1 only right, 2 different, 120 the same".
[[nodiscard]] QString compareSummary(const QList<CompareRow> &rows);

} // namespace fe
