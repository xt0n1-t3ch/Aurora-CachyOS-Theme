// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Groups the URIs of org.freedesktop.FileManager1 calls into the windows and folders to open.

#pragma once

#include <QList>
#include <QStringList>
#include <QUrl>

namespace fe {

/// One window for org.freedesktop.FileManager1.ShowItems: a folder and the items to select in it.
struct Reveal {
    QUrl folder;
    QList<QUrl> items;
};

/// The windows ShowItems opens: one per parent folder, in the order the folders first appear, each selecting
/// its items in the order given. A folder item is selected in its own parent, as "Show in folder" means.
[[nodiscard]] QList<Reveal> revealGroups(const QStringList &uris);

/// The folders ShowFolders opens, each once, in the order given.
[[nodiscard]] QList<QUrl> foldersToShow(const QStringList &uris);

} // namespace fe
