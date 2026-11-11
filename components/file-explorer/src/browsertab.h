// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// One tab of a window: Dolphin's view inside a widget, with its own history, search and status counts.

#pragma once

#include "core/format.h"
#include "core/history.h"
#include "toolbar.h"

#include <KFileItem>
#include <KParts/NavigationExtension>

#include <QHash>
#include <QPointer>
#include <QWidget>

class KNewFileMenu;
class QTimer;

namespace fe {
class FilterBar;
class SearchScopeBar;
}

namespace KParts {
class OpenUrlArguments;
class ReadOnlyPart;
}

namespace fe {

/// One tab of the window: Dolphin's view (dolphinpart) on the window glass, with the tab's own
/// history, search and status counts.
class BrowserTab : public QWidget
{
    Q_OBJECT

public:
    /// Loads Dolphin's part and opens url. When the part cannot be loaded the tab shows an error message instead.
    explicit BrowserTab(const QUrl &url, QWidget *parent = nullptr);

    /// Whether Dolphin's part loaded.
    [[nodiscard]] bool isLoaded() const;
    /// The location on screen; while a search runs, the search.
    [[nodiscard]] QUrl url() const;
    /// The folder the tab is in; while a search runs, the folder being searched.
    [[nodiscard]] QUrl folderUrl() const;
    [[nodiscard]] bool isSearching() const;
    [[nodiscard]] QString searchTerm() const;

    /// Shows url and records it in the history. Ends a search.
    void openFolder(const QUrl &url);
    /// Back from a search returns to the folder searched; otherwise it steps back in the history.
    void goBack();
    void goForward();
    void goUp();
    void reload();
    /// Selects these items once the folder has loaded (DolphinPart's filesToSelect).
    void selectItems(const QList<QUrl> &items);
    /// Selects these items of the folder on screen: it is listed again, and they are selected when it is.
    void selectInPlace(const QList<QUrl> &items);
    /// In an Open or Save panel a double-clicked file is picked, not opened in its application.
    void setPicking(bool picking);
    [[nodiscard]] bool canGoBack() const;
    [[nodiscard]] bool canGoForward() const;
    [[nodiscard]] bool canGoUp() const;

    /// Searches file names: the whole PC or the folder, from aurora-search's index as each key is typed, or, without
    /// the index, below the folder after a short pause in typing. An empty term ends the search.
    void search(const QString &term);

    void setViewMode(ViewMode mode);
    [[nodiscard]] ViewMode viewMode() const;
    /// The action of Dolphin's part with this name, or null when the part has none.
    [[nodiscard]] QAction *partAction(const QString &name) const;
    /// Triggers the part's action of this name when it exists and is enabled.
    void triggerPartAction(const QString &name) const;
    /// The actions of Dolphin's version control plugin among a context menu's action groups: DolphinPart puts them in
    /// "editactions" with its own Move to Trash, Delete and Rename (DolphinPart::slotOpenContextMenu, dolphinpart.cpp,
    /// v26.08.3, line 386), which the menu already offers and are left out here.
    [[nodiscard]] QList<QAction *> versionControlActions(const KParts::NavigationExtension::ActionGroupMap &groups) const;
    /// The part's New menu, or null without a part.
    [[nodiscard]] KNewFileMenu *newFileMenu() const;
    [[nodiscard]] KFileItemList selectedItems() const;
    [[nodiscard]] bool canCut() const;
    [[nodiscard]] bool canCopy() const;
    [[nodiscard]] bool canPaste() const;
    /// The status row text: the part's message when it sent one, else the item counts and free space.
    [[nodiscard]] QString statusText() const;

    /// Opens items the way a double click does: the first folder here, other folders in new tabs,
    /// files in their applications.
    void activateItems(const KFileItemList &items);
    /// Cut, Copy and Paste do nothing unless the part has enabled the matching action (canCut and the others).
    void cut();
    void copy();
    void paste();
    void focusView();

    /// Whether a double-clicked archive (zip, tar, 7z) opens as a folder here, as in Windows' File Explorer, instead of
    /// in its application; kept in file-explorerrc, on by default.
    [[nodiscard]] static bool opensArchivesAsFolders();
    static void setOpensArchivesAsFolders(bool asFolders);
    /// The folder view of the archive at url (`tar:/path/a.tar.gz`), or an empty URL when url is not an archive the
    /// archive worker opens or archives open in their application. mimeType may be empty; it is then read.
    [[nodiscard]] static QUrl archiveFolderUrl(const QUrl &url, const QString &mimeType);
    /// Puts the paths of every selected item on the clipboard, one per line; with nothing selected, the folder's path.
    /// Dolphin's Copy Location action (Ctrl+Alt+C) comes here.
    void copyPaths();
    /// Renames the selection: one item in place, as Dolphin does; several in the Rename sheet (BatchRenameSheet), whose
    /// one job Undo takes back whole. Dolphin's Rename action (F2) comes here.
    void renameSelection();
    /// Opens the filter row: the view keeps only the items whose names contain what is typed in it. The filter
    /// belongs to the listing on screen and ends when the tab shows another folder or search.
    void openFilter();
    /// The filter's text; empty when no filter is on.
    [[nodiscard]] QString filterText() const;
    [[nodiscard]] FilterBar *filterBar() const;

Q_SIGNALS:
    /// The location on screen changed.
    void locationChanged();
    /// The status text changed.
    void statusChanged();
    /// The part enabled or disabled Cut, Copy or Paste.
    void editStateChanged();
    /// The view mode changed, for example on moving to a folder with another saved mode.
    void viewModeChanged();
    /// A folder should open in a new tab.
    void newTabRequested(const QUrl &url);
    /// A file was double-clicked while picking.
    void filePicked(const QUrl &url);
    /// Dolphin asks for a context menu at globalPos.
    void contextMenuRequested(const QPoint &globalPos, const KFileItemList &items,
                              const KParts::NavigationExtension::ActionGroupMap &groups);

private:
    void connectPart();
    /// Shows url in the part without touching the history.
    void showUrl(const QUrl &url);
    void endSearch();
    void runSearch();
    /// Opens a folder in this tab, reports a file as picked, or starts the file in its application.
    void handleOpenRequest(const QUrl &url, const KParts::OpenUrlArguments &arguments);
    void openExternally(const QUrl &url, const QString &mimeType);
    void makeViewTransparent();
    /// Reads the item and selection counts from the part and sends statusChanged.
    void refreshCounts();
    /// Asks for the free space of the folder; the answer arrives later and sends statusChanged.
    void refreshFreeSpace();

    QPointer<KParts::ReadOnlyPart> m_part;
    QPointer<KParts::NavigationExtension> m_extension;
    NavigationHistory m_history;
    QUrl m_searchOrigin;
    bool m_picking = false;
    QString m_searchTerm;
    QTimer *m_searchTimer;
    SearchScopeBar *m_scopeBar;
    FilterBar *m_filterBar;
    /// The scope chosen in the scope row: the whole PC, or the folder the search started in.
    bool m_everywhere = true;
    /// Whether the search on screen lists from the index, which announces its changes.
    bool m_indexed = false;
    /// Gathers the index's change notices into one listing again.
    QTimer *m_relistTimer;
    QHash<QByteArray, bool> m_enabledActions;
    FolderCounts m_counts;
    QString m_message;
};

} // namespace fe
