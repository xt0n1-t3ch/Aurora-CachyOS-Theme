// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Rules for the sidebar: the context menu of a row, folders added to it, drag targets and row titles.

#pragma once

#include "core/paths.h"

#include <QList>
#include <QStringList>
#include <QString>
#include <QUrl>

#include <functional>

namespace fe {

/// A sidebar row, as far as its context menu is concerned: where it leads and what KDE's places
/// model says it can do (KFilePlacesModel::isEjectAllowed, isTeardownAllowed and FixedDeviceRole).
struct SidebarPlace {
    QUrl url;
    /// A folder the user put in the sidebar; the standard folders, home and the drives are not.
    bool addedByUser = false;
    /// A disc whose tray can open.
    bool ejectAllowed = false;
    /// A mounted drive that can be unmounted now.
    bool teardownAllowed = false;
    /// A drive built into the machine: neither hotpluggable nor removable.
    bool fixedDrive = false;
    /// For the Trash: whether it holds nothing.
    bool trashEmpty = true;
    /// A drive, or a folder the user added: its sidebar name can change.
    bool renamable = false;
};

/// What a sidebar row's context menu can do.
enum class SidebarCommand {
    Open,
    OpenInNewTab,
    GetInfo,
    Rename,
    Eject,
    Unmount,
    RemoveFromSidebar,
    EmptyTrash,
};

/// One entry of a sidebar row's context menu.
struct SidebarMenuEntry {
    SidebarCommand command;
    bool enabled = true;
};

/// A sidebar row's context menu in Finder's order, as groups drawn with separators between them: open
/// here or in a new tab; Get Info for a place with a local folder, and Rename for a drive or a folder the
/// user added; then eject or unmount a drive (a removable drive is ejected, a built-in one unmounted, the
/// system disk neither), remove a folder the user added, or empty the Trash.
[[nodiscard]] QList<QList<SidebarMenuEntry>> sidebarMenu(const SidebarPlace &place);

/// The menu text of command on the row titled title, in Finder's wording: Eject "Name".
[[nodiscard]] QString sidebarCommandText(SidebarCommand command, const QString &title);

/// The folders Add to Sidebar puts in the sidebar: the selected folders, or the current folder when no
/// folder is selected; each once, without a trailing slash, and none that the sidebar already shows.
[[nodiscard]] QList<QUrl> foldersToAdd(const QList<QUrl> &selectedFolders, const QUrl &currentFolder,
                                       const std::function<bool(const QUrl &)> &inSidebar);

/// Where a drag over a sidebar row lands.
enum class SidebarDrop {
    Nowhere,
    /// Onto the row: the files move or copy into the place, or go to the Trash.
    IntoPlace,
    /// Between two rows of Favorites: the dragged folders join the sidebar above the row.
    BeforeRow,
    /// Below the row.
    AfterRow,
};

/// What a drag at height y (0 at the top) over a row height tall does. Folders dragged near the top or
/// bottom edge of a Favorites row join the sidebar there, as in Finder; anywhere else over a place that
/// takes files, they go into it. A Favorites row that takes no files (Recents) takes folders above or
/// below it.
[[nodiscard]] SidebarDrop sidebarDrop(qreal y, qreal height, bool inFavorites, bool acceptsFiles, bool onlyFolders);

/// Where a sidebar row dragged over another row at height y (0 at the top) of a row height tall lands: above it in
/// its upper half, below it in its lower half; nowhere when the row is in the other section, since Favorites and
/// Locations keep their own rows.
[[nodiscard]] SidebarDrop sidebarRowDrop(qreal y, qreal height, bool sameSection);

/// The order a section's rows show in: keys names each row in the section's own order, saved is the order the user
/// left by dragging rows. Rows saved keep that order; rows saved does not name (places added since) follow in their
/// own order; keys in saved that no row has are skipped. Returns the rows' positions in keys.
[[nodiscard]] QList<qsizetype> sidebarOrder(const QStringList &keys, const QStringList &saved);

/// keys after the row at from is dragged next to the row at to: right above it, or right below it.
[[nodiscard]] QStringList movedSidebarRow(QStringList keys, qsizetype from, qsizetype to, bool below);

/// The name a folder gets in the sidebar: a local folder's title in the path bar (the user's name for
/// home, the system's name for the disk), a remote folder's last segment, or its host at the top.
[[nodiscard]] QString placeTitle(const QUrl &url, const PathContext &context);

} // namespace fe
