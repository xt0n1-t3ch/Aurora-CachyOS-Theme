// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The folder tree under a sidebar favorite: which subfolders show while their parents are expanded.

#pragma once

#include <QList>
#include <QSet>
#include <QString>

#include <functional>

namespace fe {

/// How deep a tree goes at most, so a folder that links to itself cannot grow it without end.
inline constexpr int TREE_DEPTH_LIMIT = 16;

/// A subfolder row under a favorite.
struct TreeEntry {
    QString path;
    QString name;
    /// 1 for a subfolder of the favorite, 2 for one of its subfolders, and so on.
    int depth = 1;
    bool expanded = false;
};

/// The rows under root, in order: when root is expanded, its subfolders, each followed by its own rows while it is
/// expanded too. subfolders lists the names of a folder's subfolders in the order they show.
[[nodiscard]] QList<TreeEntry> expandedRows(const QString &root, const QSet<QString> &expanded,
                                            const std::function<QStringList(const QString &)> &subfolders);

} // namespace fe
