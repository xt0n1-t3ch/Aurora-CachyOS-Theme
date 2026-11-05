// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements mainwindow.h except the menus, which are built in menubar.cpp.

#include "mainwindow.h"

#include "bottombar.h"
#include "browsertab.h"
#include "core/places.h"
#include "sidebar.h"
#include "style.h"
#include "tabbar.h"
#include "toolbar.h"
#include "windowcontrols.h"

#include <KPropertiesDialog>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

constexpr QSize DEFAULT_SIZE{1280, 800};
constexpr QSize MINIMUM_SIZE{760, 440};

QUrl homeUrl()
{
    return QUrl::fromLocalFile(QDir::homePath());
}

} // namespace

MainWindow::MainWindow(const QUrl &url, fe::PathContext context, QWidget *parent)
    : GlassWindow(parent)
    , m_context(std::move(context))
    , m_sidebar(new fe::SidebarPanel(m_context, this))
    , m_controls(new WindowControls)
    , m_sidebarButton(new fe::GlassButton(QStringLiteral(":/icons/sidebar.svg"), QStringLiteral("Hide Sidebar")))
    , m_newTabButton(new fe::GlassButton(QStringLiteral(":/icons/tab-new.svg"), QStringLiteral("New Tab")))
    , m_toolbar(new fe::Toolbar)
    , m_tabBar(new fe::PillTabBar)
    , m_stack(new QStackedWidget)
    , m_status(new fe::StatusRow)
    , m_pathBar(new fe::PathBar)
{
    resize(DEFAULT_SIZE);
    setMinimumSize(MINIMUM_SIZE);

    QHBoxLayout *header = m_sidebar->headerLayout();
    header->addWidget(m_sidebarButton);
    header->addWidget(m_newTabButton);
    header->addStretch();
    // Windows order at the right end, so every app on the desktop puts its buttons in the same place.
    m_toolbar->trailingLayout()->addWidget(m_controls);
    m_sidebar->view()->setFocusPolicy(Qt::NoFocus);

    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(6, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(m_toolbar);
    contentLayout->addWidget(m_tabBar);
    contentLayout->addSpacing(4);
    contentLayout->addWidget(m_stack, 1);
    contentLayout->addWidget(m_status);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(fe::style::SIDEBAR_INSET, fe::style::SIDEBAR_INSET, 0, 0);
    body->setSpacing(0);
    body->addWidget(m_sidebar);
    body->addWidget(content, 1);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(body, 1);
    root->addWidget(m_pathBar);

    addDragSurface(m_toolbar);
    addDragSurface(m_sidebar->header());

    connect(m_controls, &WindowControls::minimizeClicked, this, &QWidget::showMinimized);
    connect(m_controls, &WindowControls::maximizeClicked, this, &GlassWindow::toggleMaximized);
    connect(m_controls, &WindowControls::closeClicked, this, &QWidget::close);
    connect(m_sidebarButton, &QAbstractButton::clicked, this, [this] { setSidebarVisible(!m_sidebar->isVisible()); });
    connect(m_newTabButton, &QAbstractButton::clicked, this, [this] { openTab(homeUrl()); });

    connect(m_toolbar, &fe::Toolbar::backClicked, this, [this] { currentTab()->goBack(); });
    connect(m_toolbar, &fe::Toolbar::forwardClicked, this, [this] { currentTab()->goForward(); });
    connect(m_toolbar, &fe::Toolbar::upClicked, this, [this] { currentTab()->goUp(); });
    connect(m_toolbar, &fe::Toolbar::newFolderClicked, this, [this] { currentTab()->triggerPartAction(QStringLiteral("create_dir")); });
    connect(m_toolbar, &fe::Toolbar::viewModeChosen, this, [this](fe::ViewMode mode) { currentTab()->setViewMode(mode); });
    connect(m_toolbar, &fe::Toolbar::addToSidebarClicked, this, &MainWindow::addSelectionToSidebar);
    connect(m_toolbar, &fe::Toolbar::searchChanged, this, [this](const QString &term) { currentTab()->search(term); });
    connect(m_toolbar, &fe::Toolbar::sortClicked, this, [this] {
        const QAction *sort = currentTab()->partAction(QStringLiteral("sort"));
        if (sort && sort->menu()) {
            const QWidget *button = m_toolbar->sortButton();
            sort->menu()->popup(button->mapToGlobal(QPoint(0, button->height() + 6)));
        }
    });
    // Escape in the search field ends the search and gives the keyboard back to the files.
    auto *escape = new QAction(m_toolbar->searchField());
    escape->setShortcut(Qt::Key_Escape);
    escape->setShortcutContext(Qt::WidgetShortcut);
    m_toolbar->searchField()->addAction(escape);
    connect(escape, &QAction::triggered, this, [this] {
        m_toolbar->searchField()->clear();
        currentTab()->focusView();
    });
    connect(m_toolbar->searchField(), &QLineEdit::returnPressed, this, [this] { currentTab()->focusView(); });

    connect(m_sidebar->view(), &fe::SidebarView::placeActivated, this, &MainWindow::openFolderHere);
    // A renamed system disk starts the path bar and the tab titles with its new name at once.
    connect(m_sidebar->view(), &fe::SidebarView::systemNameChanged, this, [this](const QString &name) {
        m_context.rootLabel = name;
        syncFromTab();
    });
    connect(m_sidebar->view(), &fe::SidebarView::newTabRequested, this, [this](const QUrl &place) { openTab(place, false); });
    // A place added or removed changes what Add to Sidebar can add.
    connect(m_sidebar->view()->model(), &QAbstractItemModel::modelReset, this, &MainWindow::syncEditActions);
    connect(m_pathBar, &fe::PathBar::crumbClicked, this, &MainWindow::openFolderHere);
    connect(m_tabBar, &fe::PillTabBar::tabClicked, this, &MainWindow::setCurrentTab);
    connect(m_tabBar, &fe::PillTabBar::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabBar, &fe::PillTabBar::newTabClicked, this, [this] { openTab(homeUrl()); });
    connect(m_status, &fe::StatusRow::hiddenToggled, this, [this] { currentTab()->triggerPartAction(QStringLiteral("show_hidden_files")); syncStatus(); });
    connect(m_status, &fe::StatusRow::previewsToggled, this, [this] { currentTab()->triggerPartAction(QStringLiteral("show_preview")); syncStatus(); });
    connect(m_status, &fe::StatusRow::infoClicked, this, &MainWindow::showInfo);

    m_actions.inSidebar = [this](const QUrl &folder) { return m_sidebar->view()->showsFolder(folder); };
    m_actions.addToSidebar = [this](const QList<QUrl> &folders) {
        for (const QUrl &folder : folders) {
            m_sidebar->view()->addFolder(folder, fe::placeTitle(folder, m_context));
        }
    };
    buildMenuBar();
    qApp->installEventFilter(this);
    openTab(url);
}

fe::BrowserTab *MainWindow::currentTab() const
{
    return qobject_cast<fe::BrowserTab *>(m_stack->currentWidget());
}

void MainWindow::openTab(const QUrl &url, bool activate)
{
    auto *tab = new fe::BrowserTab(url, m_stack);
    tab->setPicking(m_picking);
    connect(tab, &fe::BrowserTab::filePicked, this, &MainWindow::filePicked);
    const int index = m_stack->addWidget(tab);
    m_tabBar->insertTab(index, fe::folderTitle(tab->url(), m_context));

    connect(tab, &fe::BrowserTab::locationChanged, this, [this, tab] {
        m_tabBar->setTabTitle(m_stack->indexOf(tab), fe::folderTitle(tab->url(), m_context));
        if (tab == currentTab()) {
            syncFromTab();
        }
    });
    connect(tab, &fe::BrowserTab::statusChanged, this, [this, tab] {
        if (tab == currentTab()) {
            syncStatus();
        }
    });
    connect(tab, &fe::BrowserTab::editStateChanged, this, [this, tab] {
        if (tab == currentTab()) {
            syncEditActions();
        }
    });
    connect(tab, &fe::BrowserTab::viewModeChanged, this, [this, tab] {
        if (tab == currentTab()) {
            m_toolbar->setViewMode(tab->viewMode());
        }
    });
    connect(tab, &fe::BrowserTab::newTabRequested, this, [this](const QUrl &target) { openTab(target, false); });
    connect(tab, &fe::BrowserTab::contextMenuRequested, this,
            [this, tab](const QPoint &pos, const KFileItemList &items, const KParts::NavigationExtension::ActionGroupMap &groups) {
                fe::execContextMenu(tab, pos, items, groups, m_actions, this);
            });

    if (activate || m_stack->count() == 1) {
        setCurrentTab(index);
    }
}

void MainWindow::setCurrentTab(int index)
{
    if (index < 0 || index >= m_stack->count()) {
        return;
    }
    m_stack->setCurrentIndex(index);
    m_tabBar->setCurrentIndex(index);
    syncFromTab();
    currentTab()->focusView();
}

void MainWindow::closeTab(int index)
{
    if (m_stack->count() <= 1) {
        close();
        return;
    }
    QWidget *tab = m_stack->widget(index);
    m_stack->removeWidget(tab);
    m_tabBar->removeTab(index);
    tab->deleteLater();
    setCurrentTab(m_tabBar->currentIndex());
}

void MainWindow::syncFromTab()
{
    fe::BrowserTab *tab = currentTab();
    if (!tab) {
        return;
    }
    const QString title = fe::folderTitle(tab->url(), m_context);
    m_toolbar->setTitle(title);
    setWindowTitle(title);
    m_toolbar->setNavigationState(tab->canGoBack(), tab->canGoForward(), tab->canGoUp());
    m_toolbar->setViewMode(tab->viewMode());
    m_toolbar->setSearchText(tab->searchTerm());
    m_sidebar->view()->setCurrentFolder(tab->folderUrl());
    m_pathBar->setCrumbs(fe::crumbsFor(tab->folderUrl(), m_context));
    syncStatus();
    syncEditActions();
}

void MainWindow::syncStatus()
{
    fe::BrowserTab *tab = currentTab();
    if (!tab) {
        return;
    }
    m_status->setText(tab->statusText());
    const QAction *hidden = tab->partAction(QStringLiteral("show_hidden_files"));
    const QAction *previews = tab->partAction(QStringLiteral("show_preview"));
    m_status->setHiddenShown(hidden && hidden->isChecked());
    m_status->setPreviewsShown(previews && previews->isChecked());
}

void MainWindow::syncEditActions()
{
    const fe::BrowserTab *tab = currentTab();
    if (!tab || !m_actions.cut) {
        return;
    }
    m_actions.cut->setEnabled(tab->canCut());
    m_actions.copy->setEnabled(tab->canCopy());
    m_actions.paste->setEnabled(tab->canPaste());
    const bool canAddToSidebar = !foldersToAddNow().isEmpty();
    m_addToSidebar->setEnabled(canAddToSidebar);
    m_toolbar->setAddToSidebarEnabled(canAddToSidebar);
}

QList<QUrl> MainWindow::foldersToAddNow() const
{
    const fe::BrowserTab *tab = currentTab();
    // Folders in the Trash are no places to go back to.
    if (!tab || tab->folderUrl().scheme() == u"trash") {
        return {};
    }
    QList<QUrl> selectedFolders;
    const KFileItemList selected = tab->selectedItems();
    for (const KFileItem &item : selected) {
        if (item.isDir()) {
            selectedFolders.append(item.targetUrl());
        }
    }
    return fe::foldersToAdd(selectedFolders, tab->folderUrl(), m_actions.inSidebar);
}

void MainWindow::addSelectionToSidebar()
{
    m_actions.addToSidebar(foldersToAddNow());
}

void MainWindow::setSidebarVisible(bool visible)
{
    // The sidebar button lives in the sidebar header; with the sidebar hidden it moves to the toolbar.
    QHBoxLayout *header = m_sidebar->headerLayout();
    QHBoxLayout *leading = m_toolbar->leadingLayout();
    if (visible) {
        leading->removeWidget(m_sidebarButton);
        leading->setContentsMargins(0, 0, 0, 0);
        header->insertWidget(0, m_sidebarButton);
    } else {
        header->removeWidget(m_sidebarButton);
        leading->setContentsMargins(10, 0, 4, 0);
        leading->addWidget(m_sidebarButton);
    }
    m_sidebar->setVisible(visible);
    update();
    m_sidebarButton->setToolTip(visible ? QStringLiteral("Hide Sidebar") : QStringLiteral("Show Sidebar"));
    if (m_sidebarAction) {
        m_sidebarAction->setText(visible ? QStringLiteral("Hide Sidebar") : QStringLiteral("Show Sidebar"));
    }
}

void MainWindow::showInfo()
{
    fe::BrowserTab *tab = currentTab();
    const KFileItemList items = tab->selectedItems();
    if (items.isEmpty()) {
        KPropertiesDialog::showDialog(tab->folderUrl(), this, false);
    } else {
        KPropertiesDialog::showDialog(items, this, false);
    }
}

void MainWindow::goToFolder()
{
    fe::BrowserTab *tab = currentTab();
    const QString current = tab->folderUrl().toDisplayString(QUrl::PreferLocalFile);
    bool accepted = false;
    QString path = QInputDialog::getText(this, QStringLiteral("Go to Folder"), QStringLiteral("Enter a path:"), QLineEdit::Normal,
                                         current, &accepted);
    path = path.trimmed();
    if (!accepted || path.isEmpty()) {
        return;
    }
    if (path == u"~" || path.startsWith(u"~/")) {
        path = QDir::homePath() + path.mid(1);
    }
    openFolderHere(QUrl::fromUserInput(path, QDir::homePath(), QUrl::AssumeLocalFile));
}

void MainWindow::openFolderHere(const QUrl &url)
{
    fe::BrowserTab *tab = currentTab();
    tab->openFolder(url);
    tab->focusView();
}

void MainWindow::openNewWindow()
{
    auto *window = new MainWindow(homeUrl(), m_context);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
}

void MainWindow::changeEvent(QEvent *event)
{
    GlassWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        m_controls->setMaximized(isMaximized());
    }
}

QPainterPath MainWindow::glassCutout() const
{
    // The sidebar is its own, clearer sheet of glass; the window tint must not darken it twice.
    QPainterPath sidebar;
    if (m_sidebar->isVisible()) {
        sidebar.addRoundedRect(QRectF(m_sidebar->geometry()), fe::style::SIDEBAR_RADIUS, fe::style::SIDEBAR_RADIUS);
    }
    return sidebar;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // The mouse's back and forward buttons, anywhere in this window. Dolphin's view takes them
    // without telling the part, so they are caught before it.
    if (event->type() == QEvent::MouseButtonPress && watched->isWidgetType()) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        const bool back = mouse->button() == Qt::BackButton;
        if ((back || mouse->button() == Qt::ForwardButton) && static_cast<QWidget *>(watched)->window() == this) {
            if (fe::BrowserTab *tab = currentTab()) {
                back ? tab->goBack() : tab->goForward();
            }
            return true;
        }
    }
    return GlassWindow::eventFilter(watched, event);
}

void MainWindow::selectItems(const QList<QUrl> &items)
{
    currentTab()->selectItems(items);
}

void MainWindow::setPicking(bool picking)
{
    m_picking = picking;
    for (int i = 0; i < m_stack->count(); ++i) {
        if (auto *tab = qobject_cast<fe::BrowserTab *>(m_stack->widget(i))) {
            tab->setPicking(picking);
        }
    }
}

void MainWindow::addBottomWidget(QWidget *widget)
{
    if (auto *root = qobject_cast<QVBoxLayout *>(layout())) {
        root->addWidget(widget);
    }
}

fe::BrowserTab *MainWindow::visibleTab() const
{
    return currentTab();
}
