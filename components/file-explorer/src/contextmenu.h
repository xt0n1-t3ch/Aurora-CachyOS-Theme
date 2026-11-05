// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The Finder-style context menus for files and folders, and the Terminal and Trash helpers they use.

#pragma once

#include <KFileItem>
#include <KParts/NavigationExtension>

#include <functional>

class QAction;
class QPoint;
class QWidget;

namespace fe {

class BrowserTab;

/// The window's actions that the context menus share with the menu bar and its shortcuts.
struct WindowActions {
    QAction *cut;
    QAction *copy;
    QAction *paste;
    /// Whether the sidebar already shows a folder.
    std::function<bool(const QUrl &)> inSidebar;
    /// Puts folders at the end of the sidebar's Favorites.
    std::function<void(const QList<QUrl> &)> addToSidebar;
};

/// Shows the Finder-style context menu at globalPos: for the items, or for the folder itself when
/// Dolphin passes the folder as the only item (a click on the empty background). Does nothing without items.
/// window is the parent of the menu and of the dialogs it starts.
void execContextMenu(BrowserTab *tab, const QPoint &globalPos, const KFileItemList &items,
                     const KParts::NavigationExtension::ActionGroupMap &groups, const WindowActions &actions, QWidget *window);

/// Opens the system terminal in a local folder.
void openTerminalAt(const QString &path, QWidget *window);

/// Empties the Trash after asking, as Dolphin does.
void emptyTrash(QWidget *window);

} // namespace fe
