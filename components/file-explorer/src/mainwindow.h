// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The File Explorer window, which holds the sidebar, toolbar, tabs, status row, path bar and menus.

#pragma once

#include "contextmenu.h"
#include "core/paths.h"
#include "glasswindow.h"

#include <QElapsedTimer>
#include <QKeySequence>

#include <functional>

class QAction;
class QMenu;
class QMenuBar;
class QStackedWidget;
class WindowControls;

namespace fe {
class ActionBar;
class BrowserTab;
class CommandPalette;
class InspectorPanel;
class GlassButton;
class PathBar;
class PillTabBar;
class QuickLook;
class SidebarPanel;
class TabPage;
class Toolbar;
enum class CrumbOpen;
enum class FileAction;
}

/// A Finder window: the glass sidebar, the toolbar with the address field, the row of file actions, tabs of
/// Dolphin views (two side by side in Split View) and the path bar, with its menus exported to the global menu bar.
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

    /// The active pane of the tab on screen.
    [[nodiscard]] fe::BrowserTab *currentTab() const;
    [[nodiscard]] fe::TabPage *currentPage() const;
    /// Follows a pane's signals; a pane reports to the window only while it is the active one.
    void connectTab(fe::BrowserTab *tab);
    void setCurrentTab(int index);
    /// Closes the tab at index; closing the last tab closes the window.
    void closeTab(int index);
    /// Refreshes the title, toolbar, sidebar, path bar and action row from the current tab.
    void syncFromTab();
    /// The path bar: the folder, or the one selected item, and the item count.
    void syncStatus();
    /// Enables Cut, Copy, Paste, Add to Sidebar and the action row as the current tab allows.
    void syncEditActions();
    /// Copies the enablement, check state and shortcuts of the current tab's part actions to their menu entries.
    void syncPartProxies();
    void setSidebarVisible(bool visible);
    /// Opens or closes the Inspector and remembers it for the next window.
    void setInspectorVisible(bool visible);
    void showInfo();
    /// The folders Add to Sidebar would add now: the selected folders, or the folder on screen.
    [[nodiscard]] QList<QUrl> foldersToAddNow() const;
    void addSelectionToSidebar();
    /// Puts the keyboard in the address field with the path selected.
    void goToFolder();
    /// Goes where the address field's text leads: a folder opens, a file opens its folder with the file selected,
    /// and a path that leads nowhere rings the field red.
    void goToAddress(const QString &text);
    /// Carries out a button of the action row.
    void runAction(fe::FileAction action);
    /// Splits the tab on screen into two panes, or closes the other pane.
    void toggleSplit();
    /// Compares the two folders of Split View in a Compare Folders sheet; without a split, opens the second pane first.
    void compareFolders();
    /// Shows the selected item in Quick Look, or closes Quick Look when it shows it already.
    void quickLook();
    [[nodiscard]] QList<QUrl> selectedUrls() const;
    // The menus of the toolbar's buttons, in windowmenus.cpp.
    void showGroupMenu();
    void showShareMenu();
    void showTagsMenu();
    void showMoreMenu();
    /// Opens url in the current tab and gives the keyboard to its view.
    void openFolderHere(const QUrl &url);
    /// Opens a path segment of the address field or the path bar: here or in a new tab; the selected file the path bar
    /// can end with opens like a double click.
    void openCrumb(const QUrl &url, fe::CrumbOpen how);
    /// The menu of a path segment at globalPos: open in a new tab, copy the path, a terminal there, Get Info.
    void showCrumbMenu(const QUrl &url, const QPoint &globalPos);
    /// Whether url is the selected file, which the path bar shows as its last segment.
    [[nodiscard]] bool isSelectedFile(const QUrl &url) const;
    /// Shows the command palette with every command of the menus, refreshed for the tab on screen.
    void showCommandPalette();
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
    fe::ActionBar *m_actionBar;
    fe::PillTabBar *m_tabBar;
    QStackedWidget *m_stack;
    fe::PathBar *m_pathBar;
    fe::InspectorPanel *m_inspector;
    QAction *m_inspectorAction = nullptr;
    fe::QuickLook *m_quickLook = nullptr;
    /// Since the last letter typed in a file view, for Space to tell Quick Look from a name being typed.
    QElapsedTimer m_lastTyped;
    fe::WindowActions m_actions{};
    QAction *m_sidebarAction = nullptr;
    QAction *m_addToSidebar = nullptr;
    QAction *m_filterAction = nullptr;
    QAction *m_paletteAction = nullptr;
    QAction *m_columnsAction = nullptr;
    QAction *m_galleryAction = nullptr;
    QAction *m_compareAction = nullptr;
    /// The menu bar, hidden without a global menu; the command palette lists its menus.
    QMenuBar *m_menuBar = nullptr;
    fe::CommandPalette *m_palette = nullptr;
    QMenu *m_sortMenu = nullptr;
    QList<PartProxy> m_partProxies;
    bool m_picking = false;
};
