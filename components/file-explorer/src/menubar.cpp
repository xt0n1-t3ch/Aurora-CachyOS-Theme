// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The menus of MainWindow: exported to Plasma's global menu bar, and the window's keyboard shortcuts.

#include "mainwindow.h"

#include "browsertab.h"
#include "sidebar.h"
#include "toolbar.h"

#include <QAction>
#include <QDir>
#include <QMenu>
#include <QMenuBar>
#include <QStackedWidget>
#include <QStandardPaths>

namespace {

// The user's folder for a standard location, as a URL.
QUrl standardFolder(QStandardPaths::StandardLocation location)
{
    return QUrl::fromLocalFile(QStandardPaths::writableLocation(location));
}

// Shortens the shortcut tables below.
QKeySequence keys(QKeyCombination combination)
{
    return QKeySequence(combination);
}

} // namespace

QAction *MainWindow::addWindowAction(QMenu *menu, const QString &text, const QList<QKeySequence> &shortcuts,
                                     const std::function<void()> &slot)
{
    auto *action = new QAction(text, this);
    action->setShortcuts(shortcuts);
    // The window holds the shortcut, so it works whether the menu bar is exported or hidden.
    addAction(action);
    menu->addAction(action);
    connect(action, &QAction::triggered, this, slot);
    return action;
}

QAction *MainWindow::addPartProxy(QMenu *menu, const QString &text, const QString &partActionName)
{
    QAction *action = menu->addAction(text);
    // The proxy shows the part's shortcut but never answers it: a WidgetShortcut on the menu bar
    // never matches, so the part's own action handles the key and nothing is ambiguous.
    action->setShortcutContext(Qt::WidgetShortcut);
    connect(action, &QAction::triggered, this, [this, partActionName] {
        if (fe::BrowserTab *tab = currentTab()) {
            tab->triggerPartAction(partActionName);
        }
    });
    m_partProxies.append({action, partActionName});
    return action;
}

void MainWindow::syncPartProxies()
{
    const fe::BrowserTab *tab = currentTab();
    for (const PartProxy &proxy : std::as_const(m_partProxies)) {
        const QAction *part = tab ? tab->partAction(proxy.partActionName) : nullptr;
        proxy.action->setEnabled(part && part->isEnabled());
        proxy.action->setCheckable(part && part->isCheckable());
        proxy.action->setChecked(part && part->isCheckable() && part->isChecked());
        proxy.action->setShortcuts(part ? part->shortcuts() : QList<QKeySequence>());
    }
}

void MainWindow::buildMenuBar()
{
    auto *bar = new QMenuBar(this);
    // Exported to the global menu bar when Plasma's app menu runs; otherwise kept out of sight,
    // since every action is also a window shortcut.
    if (!bar->isNativeMenuBar()) {
        bar->hide();
    }

    QMenu *file = bar->addMenu(QStringLiteral("File"));
    addWindowAction(file, QStringLiteral("New Window"), {keys(Qt::CTRL | Qt::Key_N)}, [this] { openNewWindow(); })
        ->setIcon(QIcon::fromTheme(QStringLiteral("window-new")));
    addWindowAction(file, QStringLiteral("New Tab"), {keys(Qt::CTRL | Qt::Key_T)},
                    [this] { openTab(QUrl::fromLocalFile(QDir::homePath())); })
        ->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    addPartProxy(file, QStringLiteral("New Folder"), QStringLiteral("create_dir"))->setIcon(QIcon::fromTheme(QStringLiteral("folder-new")));
    file->addSeparator();
    addWindowAction(file, QStringLiteral("Open"), {keys(Qt::CTRL | Qt::Key_O), keys(Qt::CTRL | Qt::Key_Down)}, [this] {
        currentTab()->activateItems(currentTab()->selectedItems());
    });
    file->addSeparator();
    addWindowAction(file, QStringLiteral("Get Info"), {keys(Qt::CTRL | Qt::Key_I)}, [this] { showInfo(); })
        ->setIcon(QIcon::fromTheme(QStringLiteral("document-properties")));
    addPartProxy(file, QStringLiteral("Rename…"), QStringLiteral("renamefile"));
    addPartProxy(file, QStringLiteral("Duplicate"), QStringLiteral("duplicate"));
    // Finder's place in the File menu and its keys: ⌃⌘T.
    m_addToSidebar = addWindowAction(file, QStringLiteral("Add to Sidebar"), {keys(Qt::CTRL | Qt::META | Qt::Key_T)},
                                     [this] { addSelectionToSidebar(); });
    m_addToSidebar->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-new")));
    addPartProxy(file, QStringLiteral("Move to Trash"), QStringLiteral("movetotrash"))->setIcon(QIcon::fromTheme(QStringLiteral("user-trash")));
    file->addSeparator();
    addWindowAction(file, QStringLiteral("Close Tab"), {keys(Qt::CTRL | Qt::Key_W)}, [this] { closeTab(m_stack->currentIndex()); });
    addWindowAction(file, QStringLiteral("Close Window"), {keys(Qt::CTRL | Qt::SHIFT | Qt::Key_W)}, [this] { close(); });

    QMenu *edit = bar->addMenu(QStringLiteral("Edit"));
    m_actions.cut = addWindowAction(edit, QStringLiteral("Cut"), QKeySequence::keyBindings(QKeySequence::Cut), [this] { currentTab()->cut(); });
    m_actions.copy = addWindowAction(edit, QStringLiteral("Copy"), QKeySequence::keyBindings(QKeySequence::Copy), [this] { currentTab()->copy(); });
    m_actions.paste = addWindowAction(edit, QStringLiteral("Paste"), QKeySequence::keyBindings(QKeySequence::Paste), [this] { currentTab()->paste(); });
    m_actions.cut->setIcon(QIcon::fromTheme(QStringLiteral("edit-cut")));
    m_actions.copy->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    m_actions.paste->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
    addPartProxy(edit, QStringLiteral("Copy Path"), QStringLiteral("copy_location"));
    addPartProxy(edit, QStringLiteral("Select All"), QStringLiteral("edit_select_all"));
    edit->addSeparator();
    addWindowAction(edit, QStringLiteral("Find"), {keys(Qt::CTRL | Qt::Key_F)}, [this] {
        m_toolbar->searchField()->setFocus();
        m_toolbar->searchField()->selectAll();
    })->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));

    QMenu *view = bar->addMenu(QStringLiteral("View"));
    addPartProxy(view, QStringLiteral("as Icons"), QStringLiteral("icons"));
    addPartProxy(view, QStringLiteral("as List"), QStringLiteral("details"));
    addPartProxy(view, QStringLiteral("as Compact List"), QStringLiteral("compact"));
    view->addSeparator();
    m_sortMenu = view->addMenu(QStringLiteral("Sort By"));
    addPartProxy(view, QStringLiteral("Show Hidden Files"), QStringLiteral("show_hidden_files"));
    addPartProxy(view, QStringLiteral("Show Previews"), QStringLiteral("show_preview"));
    view->addSeparator();
    m_sidebarAction = addWindowAction(view, QStringLiteral("Hide Sidebar"), {keys(Qt::CTRL | Qt::ALT | Qt::Key_S)},
                                      [this] { setSidebarVisible(!m_sidebar->isVisible()); });
    addWindowAction(view, QStringLiteral("Reload"), {keys(Qt::Key_F5)}, [this] { currentTab()->reload(); })
        ->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    connect(view, &QMenu::aboutToShow, this, [this] {
        // Sort By lists the current tab's own sort actions, so the checks match its folder.
        m_sortMenu->clear();
        const QAction *sort = currentTab()->partAction(QStringLiteral("sort"));
        if (sort && sort->menu()) {
            m_sortMenu->addActions(sort->menu()->actions());
        }
    });

    QMenu *go = bar->addMenu(QStringLiteral("Go"));
    addWindowAction(go, QStringLiteral("Back"), {keys(Qt::CTRL | Qt::Key_BracketLeft), keys(Qt::ALT | Qt::Key_Left)},
                    [this] { currentTab()->goBack(); })
        ->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    addWindowAction(go, QStringLiteral("Forward"), {keys(Qt::CTRL | Qt::Key_BracketRight), keys(Qt::ALT | Qt::Key_Right)},
                    [this] { currentTab()->goForward(); })
        ->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    addWindowAction(go, QStringLiteral("Enclosing Folder"), {keys(Qt::CTRL | Qt::Key_Up), keys(Qt::ALT | Qt::Key_Up)},
                    [this] { currentTab()->goUp(); })
        ->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    go->addSeparator();
    const struct {
        const char *title;
        const char *icon;
        QUrl url;
        QList<QKeySequence> shortcuts;
    } places[] = {
        {"Recents", "document-open-recent", QUrl(QStringLiteral("recentlyused:/files")), {keys(Qt::CTRL | Qt::SHIFT | Qt::Key_F)}},
        {"Home", "user-home", QUrl::fromLocalFile(QDir::homePath()), {keys(Qt::CTRL | Qt::SHIFT | Qt::Key_H)}},
        {"Desktop", "user-desktop", standardFolder(QStandardPaths::DesktopLocation), {keys(Qt::CTRL | Qt::SHIFT | Qt::Key_D)}},
        {"Documents", "folder-documents", standardFolder(QStandardPaths::DocumentsLocation), {keys(Qt::CTRL | Qt::SHIFT | Qt::Key_O)}},
        {"Downloads", "folder-download", standardFolder(QStandardPaths::DownloadLocation), {keys(Qt::CTRL | Qt::SHIFT | Qt::Key_L)}},
        {"Trash", "user-trash", QUrl(QStringLiteral("trash:/")), {}},
    };
    for (const auto &place : places) {
        addWindowAction(go, QString::fromLatin1(place.title), place.shortcuts, [this, url = place.url] { openFolderHere(url); })
            ->setIcon(QIcon::fromTheme(QString::fromLatin1(place.icon)));
    }
    go->addSeparator();
    addWindowAction(go, QStringLiteral("Go to Folder…"), {keys(Qt::CTRL | Qt::SHIFT | Qt::Key_G), keys(Qt::CTRL | Qt::Key_L)},
                    [this] { goToFolder(); });

    QMenu *window = bar->addMenu(QStringLiteral("Window"));
    addWindowAction(window, QStringLiteral("Minimize"), {keys(Qt::CTRL | Qt::Key_M)}, [this] { showMinimized(); });
    addWindowAction(window, QStringLiteral("Maximize"), {}, [this] { toggleMaximized(); });
    window->addSeparator();
    addWindowAction(window, QStringLiteral("Show Previous Tab"), {keys(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab), keys(Qt::CTRL | Qt::Key_PageUp)}, [this] {
        setCurrentTab((m_stack->currentIndex() + m_stack->count() - 1) % m_stack->count());
    });
    addWindowAction(window, QStringLiteral("Show Next Tab"), {keys(Qt::CTRL | Qt::Key_Tab), keys(Qt::CTRL | Qt::Key_PageDown)}, [this] {
        setCurrentTab((m_stack->currentIndex() + 1) % m_stack->count());
    });

    for (QMenu *menu : {file, edit, view, go, window}) {
        connect(menu, &QMenu::aboutToShow, this, [this] {
            syncPartProxies();
            syncEditActions();
        });
    }
}
