// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements contextmenu.h. Dolphin's part owns the file actions, so the menus reuse them to keep their
// enablement and shortcuts.

#include "contextmenu.h"

#include "browsertab.h"
#include "core/places.h"

#include <KFileItemActions>
#include <KFileItemListProperties>
#include <KIO/DeleteOrTrashJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/RestoreJob>
#include <KNewFileMenu>
#include <KPropertiesDialog>
#include <KTerminalLauncherJob>

#include <QMenu>

namespace fe {

namespace {

QIcon icon(const char *name)
{
    return QIcon::fromTheme(QString::fromLatin1(name));
}

// Adds the action of Dolphin's part with this name; a part without it adds nothing.
void addPartAction(QMenu &menu, const BrowserTab *tab, const char *name)
{
    if (QAction *action = tab->partAction(QString::fromLatin1(name))) {
        menu.addAction(action);
    }
}

/// Add to Sidebar for the folders a menu is about, unless the sidebar shows them all already.
void addSidebarEntry(QMenu &menu, const QList<QUrl> &selectedFolders, const QUrl &currentFolder, const WindowActions &actions)
{
    const QList<QUrl> folders = foldersToAdd(selectedFolders, currentFolder, actions.inSidebar);
    if (!folders.isEmpty()) {
        menu.addAction(icon("bookmark-new"), QStringLiteral("Add to Sidebar"), &menu, [add = actions.addToSidebar, folders] { add(folders); });
    }
}

/// The menu of one or more selected items.
void fillItemMenu(QMenu &menu, BrowserTab *tab, const KFileItemList &items, const KParts::NavigationExtension::ActionGroupMap &groups,
                  KFileItemActions &fileActions, const WindowActions &actions, QWidget *window)
{
    const KFileItem first = items.constFirst();
    const bool single = items.size() == 1;
    const bool inTrash = first.url().scheme() == u"trash";

    menu.addAction(icon("document-open"), QStringLiteral("Open"), tab, [tab, items] { tab->activateItems(items); });
    if (single && first.isDir()) {
        menu.addAction(icon("tab-new"), QStringLiteral("Open in New Tab"), tab,
                       [tab, url = first.targetUrl()] { Q_EMIT tab->newTabRequested(url); });
    }
    if (!inTrash) {
        QAction *anchor = menu.addSeparator();
        fileActions.insertOpenWithActionsTo(anchor, &menu, {});
    }
    menu.addSeparator();

    if (inTrash) {
        menu.addAction(icon("edit-undo"), QStringLiteral("Put Back"), tab, [items, window] {
            KIO::RestoreJob *job = KIO::restoreFromTrash(items.urlList());
            job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
        });
    }
    // Move to Trash, Delete and Rename, with Dolphin's own enablement and shortcuts.
    const QList<QAction *> editActions = groups.value(QStringLiteral("editactions"));
    for (QAction *action : editActions) {
        if (action) {
            menu.addAction(action);
        }
    }
    if (!inTrash) {
        addPartAction(menu, tab, "duplicate");
    }
    menu.addSeparator();

    // Service menus and plugins: Compress, Extract, Share, ...
    fileActions.addActionsTo(&menu);
    menu.addSeparator();

    menu.addAction(actions.cut);
    menu.addAction(actions.copy);
    addPartAction(menu, tab, "copy_location");
    menu.addSeparator();

    menu.addAction(icon("document-properties"), QStringLiteral("Get Info\tCtrl+I"), tab,
                   [items, window] { KPropertiesDialog::showDialog(items, window, false); });
    if (!inTrash) {
        QList<QUrl> folders;
        for (const KFileItem &item : items) {
            if (item.isDir()) {
                folders.append(item.targetUrl());
            }
        }
        // With no folder among the items there is no current folder to fall back to: no entry.
        addSidebarEntry(menu, folders, QUrl(), actions);
    }
    if (single && first.isDir() && first.isLocalFile()) {
        menu.addAction(icon("utilities-terminal"), QStringLiteral("Open Terminal Here"), tab,
                       [path = first.localPath(), window] { openTerminalAt(path, window); });
    }
}

/// The menu of the folder itself, for a click on empty space.
void fillBackgroundMenu(QMenu &menu, BrowserTab *tab, KFileItemActions &fileActions, const WindowActions &actions, QWidget *window)
{
    const QUrl folder = tab->url();
    if (folder.scheme() == u"trash") {
        menu.addAction(icon("trash-empty"), QStringLiteral("Empty Trash"), tab, [window] { emptyTrash(window); });
        menu.addSeparator();
    } else if (!tab->isSearching()) {
        addPartAction(menu, tab, "create_dir");
        if (KNewFileMenu *newMenu = tab->newFileMenu()) {
            newMenu->setWorkingDirectory(folder);
            newMenu->checkUpToDate();
            menu.addAction(newMenu);
        }
        menu.addSeparator();
        menu.addAction(actions.paste);
        menu.addSeparator();
    }

    addPartAction(menu, tab, "show_hidden_files");
    addPartAction(menu, tab, "sort");
    QMenu *viewAs = menu.addMenu(icon("view-choose"), QStringLiteral("View As"));
    for (const char *name : {"icons", "details", "compact"}) {
        addPartAction(*viewAs, tab, name);
    }
    menu.addSeparator();

    if (!tab->isSearching()) {
        fileActions.addActionsTo(&menu);
        menu.addSeparator();
    }
    if (folder.isLocalFile()) {
        menu.addAction(icon("utilities-terminal"), QStringLiteral("Open Terminal Here"), tab,
                       [path = folder.toLocalFile(), window] { openTerminalAt(path, window); });
    }
    // While a search runs, the folder being searched; the search itself is no place.
    addSidebarEntry(menu, {}, tab->folderUrl(), actions);
    menu.addAction(icon("document-properties"), QStringLiteral("Get Info\tCtrl+I"), tab,
                   [folder, window] { KPropertiesDialog::showDialog(folder, window, false); });
}

} // namespace

void execContextMenu(BrowserTab *tab, const QPoint &globalPos, const KFileItemList &items,
                     const KParts::NavigationExtension::ActionGroupMap &groups, const WindowActions &actions, QWidget *window)
{
    if (items.isEmpty()) {
        return;
    }
    QMenu menu(window);
    KFileItemActions fileActions;
    fileActions.setParentWidget(window);
    fileActions.setItemListProperties(KFileItemListProperties(items));

    const bool onBackground = items.size() == 1 && items.constFirst().url().matches(tab->url(), QUrl::StripTrailingSlash);
    if (onBackground) {
        fillBackgroundMenu(menu, tab, fileActions, actions, window);
    } else {
        fillItemMenu(menu, tab, items, groups, fileActions, actions, window);
    }
    menu.exec(globalPos);
}

void openTerminalAt(const QString &path, QWidget *window)
{
    auto *job = new KTerminalLauncherJob(QString());
    job->setWorkingDirectory(path);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
    job->start();
}

void emptyTrash(QWidget *window)
{
    // The job Dolphin runs, which asks first (src/trash/dolphintrash.cpp, v26.08.2, line 91).
    using Ask = KIO::AskUserActionInterface;
    auto *job = new KIO::DeleteOrTrashJob({}, Ask::EmptyTrash, Ask::DefaultConfirmation, window);
    job->start();
}

} // namespace fe
