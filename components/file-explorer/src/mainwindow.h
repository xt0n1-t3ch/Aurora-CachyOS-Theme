// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The File Explorer window, which holds the sidebar, toolbar, tabs, status row, path bar and menus.

#pragma once

#include "contextmenu.h"
#include "core/paths.h"
#include "glasswindow.h"

#include <QKeySequence>

#include <functional>

class QAction;
class QMenu;
class QStackedWidget;
class WindowControls;

namespace fe {
class BrowserTab;
class GlassButton;
class PathBar;
class PillTabBar;
class SidebarPanel;
class StatusRow;
class Toolbar;
}

/// A Finder window: the glass sidebar, the toolbar, tabs of Dolphin views, the status row and the
/// path bar, with its menus exported to the global menu bar.
class MainWindow : public GlassWindow
{
    Q_OBJECT

public:
    /// Opens a window showing the folder url in its first tab. context names the start of paths in the path bar and
    /// the tab titles.
    MainWindow(const QUrl &url, fe::PathContext context, QWidget *parent = nullptr);

    /// Opens url in a new tab at the end of the tab bar.
    void openTab(const QUrl &url, bool activate = true);

    /// Selects these items in the current tab once its folder has loaded.
    void selectItems(const QList<QUrl> &items);

    /// Turns the window into an Open or Save panel: every tab picks files instead of opening them.
    void setPicking(bool picking);
    /// Adds a strip below the path bar (the Open or Save panel's buttons).
    void addBottomWidget(QWidget *widget);
    /// The tab on screen, for a panel to read its folder and selection.
    [[nodiscard]] fe::BrowserTab *visibleTab() const;

Q_SIGNALS:
    /// A file was double-clicked in a picking window.
    void filePicked(const QUrl &url);

protected:
    [[nodiscard]] QPainterPath glassCutout() const override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// A menu entry that triggers an action of the current tab's Dolphin part.
    struct PartProxy {
        QAction *action;
        QString partActionName;
    };

    [[nodiscard]] fe::BrowserTab *currentTab() const;
    void setCurrentTab(int index);
    /// Closes the tab at index; closing the last tab closes the window.
    void closeTab(int index);
    /// Refreshes the title, toolbar, sidebar, path bar and status row from the current tab.
    void syncFromTab();
    void syncStatus();
    /// Enables Cut, Copy, Paste and Add to Sidebar as the current tab allows.
    void syncEditActions();
    /// Copies the enablement, check state and shortcuts of the current tab's part actions to their menu entries.
    void syncPartProxies();
    void setSidebarVisible(bool visible);
    void showInfo();
    /// The folders Add to Sidebar would add now: the selected folders, or the folder on screen.
    [[nodiscard]] QList<QUrl> foldersToAddNow() const;
    void addSelectionToSidebar();
    /// Asks for a path and opens it in the current tab.
    void goToFolder();
    /// Opens url in the current tab and gives the keyboard to its view.
    void openFolderHere(const QUrl &url);
    /// Opens another window on the home folder.
    void openNewWindow();
    void buildMenuBar();
    /// Adds an action to menu and to the window, so its shortcuts work even when the menu bar is hidden.
    QAction *addWindowAction(QMenu *menu, const QString &text, const QList<QKeySequence> &shortcuts,
                             const std::function<void()> &slot);
    /// Adds a menu entry that triggers the current tab's part action of this name (see PartProxy).
    QAction *addPartProxy(QMenu *menu, const QString &text, const QString &partActionName);

    fe::PathContext m_context;
    fe::SidebarPanel *m_sidebar;
    WindowControls *m_controls;
    fe::GlassButton *m_sidebarButton;
    fe::GlassButton *m_newTabButton;
    fe::Toolbar *m_toolbar;
    fe::PillTabBar *m_tabBar;
    QStackedWidget *m_stack;
    fe::StatusRow *m_status;
    fe::PathBar *m_pathBar;
    fe::WindowActions m_actions{};
    QAction *m_sidebarAction = nullptr;
    QAction *m_addToSidebar = nullptr;
    QMenu *m_sortMenu = nullptr;
    QList<PartProxy> m_partProxies;
    bool m_picking = false;
};
