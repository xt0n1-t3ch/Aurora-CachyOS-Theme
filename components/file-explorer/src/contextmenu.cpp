// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements contextmenu.h. Dolphin's part owns the file actions, so the menus reuse them to keep their
// enablement and shortcuts.

#include "contextmenu.h"

#include "core/format.h"

#include "browsertab.h"
#include "core/places.h"
#include "gitactions.h"
#include "tagging.h"

#include <KFileItemActions>
#include <KFileItemListProperties>
#include <KIO/DeleteOrTrashJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/RestoreJob>
#include <KNewFileMenu>
#include <KPropertiesDialog>
#include <KTerminalLauncherJob>

#include <QFileInfo>
#include <QMenu>
#include <QProcess>
#include <QStandardPaths>

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

/// Ark's Compress to .zip for items, when Ark is installed: the archive lands beside the first item.
void addCompressEntry(QMenu &menu, const KFileItemList &items)
{
    const QString ark = QStandardPaths::findExecutable(QStringLiteral("ark"));
    QStringList paths;
    for (const KFileItem &item : items) {
        if (!item.isLocalFile()) {
            return;
        }
        paths.append(item.localPath());
    }
    if (ark.isEmpty() || paths.isEmpty()) {
        return;
    }
    menu.addAction(icon("archive-insert"), QStringLiteral("Compress to .zip"), &menu, [ark, paths] {
        // The command of Ark's own "Compress to zip" service menu (ark_addtoservicemenu.desktop).
        QProcess::startDetached(ark, QStringList{QStringLiteral("--changetofirstpath"), QStringLiteral("--add"), QStringLiteral("--autofilename"),
                                                 QStringLiteral("zip")}
                                         + paths);
    });
}

/// Service menus and plugins (Extract, Share, Activities, ...) in one Quick Actions submenu, as Finder groups them.
void addQuickActions(QMenu &menu, KFileItemActions &fileActions)
{
    QMenu *quick = menu.addMenu(icon("system-run"), QStringLiteral("Quick Actions"));
    fileActions.addActionsTo(quick);
    if (quick->isEmpty()) {
        menu.removeAction(quick->menuAction());
        quick->deleteLater();
    }
}

/// The menu of one or more selected items, in Finder's order.
void fillItemMenu(QMenu &menu, BrowserTab *tab, const KFileItemList &items, KFileItemActions &fileActions,
                  const WindowActions &actions, QWidget *window)
{
    const KFileItem first = items.constFirst();
    const bool single = items.size() == 1;
    const bool inTrash = first.url().scheme() == u"trash";

    menu.addAction(icon("document-open"), QStringLiteral("Open"), tab, [tab, items] { tab->activateItems(items); });
    if (!inTrash) {
        QMenu *openWith = menu.addMenu(icon("document-open"), QStringLiteral("Open With"));
        fileActions.insertOpenWithActionsTo(nullptr, openWith, {});
        if (openWith->isEmpty()) {
            menu.removeAction(openWith->menuAction());
        }
    }
    if (single && first.isDir()) {
        menu.addAction(icon("tab-new"), QStringLiteral("Open in New Tab"), tab,
                       [tab, url = first.targetUrl()] { Q_EMIT tab->newTabRequested(url); });
    }
    menu.addSeparator();

    if (inTrash) {
        menu.addAction(icon("edit-undo"), QStringLiteral("Put Back"), tab, [items, window] {
            KIO::RestoreJob *job = KIO::restoreFromTrash(items.urlList());
            job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
        });
        addPartAction(menu, tab, "deletefile");
    } else {
        addPartAction(menu, tab, "renamefile");
        addPartAction(menu, tab, "duplicate");
        addCompressEntry(menu, items);
    }
    menu.addAction(icon("view-preview"), QStringLiteral("Quick Look\tSpace"), tab, actions.quickLook);
    menu.addSeparator();

    menu.addAction(actions.cut);
    menu.addAction(actions.copy);
    // Dolphin's Copy Location under the name the menu bar and Finder use; it copies every selected path
    // (BrowserTab::copyPaths).
    if (QAction *copyPath = tab->partAction(QStringLiteral("copy_location"))) {
        const QString text = single ? QStringLiteral("Copy Path") : QStringLiteral("Copy %1 Paths").arg(items.size());
        QAction *entry = menu.addAction(copyPath->icon(), text, copyPath, &QAction::trigger);
        entry->setShortcuts(copyPath->shortcuts());
        entry->setShortcutContext(Qt::WidgetShortcut);
    }
    menu.addAction(actions.paste);
    if (!inTrash) {
        menu.addSeparator();
        addPartAction(menu, tab, "movetotrash");
        menu.addSeparator();
        QList<QUrl> urls;
        for (const KFileItem &item : items) {
            urls.append(item.targetUrl());
        }
        tagging::addTagEntries(menu, urls);
    }
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
        addQuickActions(menu, fileActions);
    }
    // A folder opens the terminal inside it; a file, in the folder it is in.
    if (first.isLocalFile()) {
        const QString path = first.isDir() ? first.localPath() : QFileInfo(first.localPath()).absolutePath();
        menu.addSeparator();
        menu.addAction(icon("utilities-terminal"), QStringLiteral("Open Terminal Here"), tab,
                       [path, window] { openTerminalAt(path, window); });
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
        addQuickActions(menu, fileActions);
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
                     const WindowActions &actions, QWidget *window, const QList<QAction *> &versionControl)
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
        fillItemMenu(menu, tab, items, fileActions, actions, window);
    }
    // Git has a menu of its own, which reads the repository of what was clicked; Dolphin's Git plugin only knows the
    // folder on screen. Other systems (Subversion, Mercurial) keep the plugin's actions.
    QList<QAction *> otherSystems;
    for (QAction *action : versionControl) {
        if (!action->parent() || !action->parent()->inherits("FileViewGitPlugin")) {
            otherSystems.append(action);
        }
    }
    if (!otherSystems.isEmpty()) {
        QStringList texts;
        for (const QAction *action : std::as_const(otherSystems)) {
            texts.append(action->text());
        }
        menu.addSeparator();
        QMenu *submenu = menu.addMenu(icon("vcs-normal"), versionControlMenuTitle(texts));
        submenu->addActions(otherSystems);
    }
    QList<git::SelectedPath> selected;
    for (const KFileItem &item : items) {
        if (item.localPath().isEmpty()) {
            selected.clear();
            break;
        }
        selected.append({item.localPath(), item.isDir()});
    }
    QPointer<BrowserTab> view(tab);
    addGitMenu(menu, {git::targetOf(selected, onBackground, gitSafeDirectories), window, [view] {
                          if (view) {
                              view->reload();
                          }
                      }});
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
