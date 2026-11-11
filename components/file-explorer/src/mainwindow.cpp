// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements mainwindow.h except the menus, which are built in menubar.cpp.

#include "mainwindow.h"

#include "actionbar.h"
#include "addressfield.h"
#include "bottombar.h"
#include "browsertab.h"
#include "contextmenu.h"
#include "crumbstrip.h"
#include "core/address.h"
#include "core/places.h"
#include "core/quicklook.h"
#include "inspectorpanel.h"
#include "quicklook.h"
#include "searchservice.h"
#include "sidebar.h"
#include "style.h"
#include "tabbar.h"
#include "tabpage.h"
#include "toolbar.h"
#include "windowcontrols.h"

#include <KConfigGroup>
#include <KNewFileMenu>
#include <KSharedConfig>
#include <KPropertiesDialog>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

constexpr QSize DEFAULT_SIZE{1440, 880};
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
    , m_toolbar(new fe::Toolbar(m_context.homePath))
    , m_actionBar(new fe::ActionBar)
    , m_tabBar(new fe::PillTabBar)
    , m_stack(new QStackedWidget)
    , m_pathBar(new fe::PathBar)
    , m_inspector(new fe::InspectorPanel)
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
    contentLayout->setContentsMargins(6, 0, 10, 0);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(m_toolbar);
    contentLayout->addSpacing(2);
    contentLayout->addWidget(m_actionBar);
    contentLayout->addSpacing(6);
    contentLayout->addWidget(m_tabBar);
    contentLayout->addSpacing(4);
    // The files and, at their right, the Inspector when it is open.
    auto *middle = new QHBoxLayout;
    middle->setContentsMargins(0, 0, 0, 0);
    middle->setSpacing(8);
    middle->addWidget(m_stack, 1);
    middle->addWidget(m_inspector);
    contentLayout->addLayout(middle, 1);
    contentLayout->addWidget(m_pathBar);
    m_inspector->setVisible(KSharedConfig::openConfig(QStringLiteral("file-explorerrc"))->group(QStringLiteral("Inspector")).readEntry("Visible", false));

    auto *body = new QHBoxLayout;
    body->setContentsMargins(fe::style::SIDEBAR_INSET, fe::style::SIDEBAR_INSET, 0, fe::style::SIDEBAR_INSET);
    body->setSpacing(0);
    body->addWidget(m_sidebar);
    body->addWidget(content, 1);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(body, 1);

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
    connect(m_toolbar, &fe::Toolbar::viewModeChosen, this, [this](fe::ViewMode mode) { currentTab()->setViewMode(mode); });
    connect(m_toolbar, &fe::Toolbar::searchChanged, this, [this](const QString &term) { currentTab()->search(term); });
    connect(m_toolbar, &fe::Toolbar::groupClicked, this, &MainWindow::showGroupMenu);
    connect(m_toolbar, &fe::Toolbar::shareClicked, this, &MainWindow::showShareMenu);
    connect(m_toolbar, &fe::Toolbar::editTagsClicked, this, &MainWindow::showTagsMenu);
    connect(m_toolbar, &fe::Toolbar::moreClicked, this, &MainWindow::showMoreMenu);
    connect(m_toolbar->addressField(), &fe::AddressField::addressEntered, this, &MainWindow::goToAddress);
    connect(m_toolbar->addressField(), &fe::AddressField::editingCancelled, this, [this] { currentTab()->focusView(); });
    connect(m_actionBar, &fe::ActionBar::triggered, this, &MainWindow::runAction);
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
    for (fe::CrumbStrip *crumbs : {m_toolbar->addressField()->crumbs(), m_pathBar->crumbs()}) {
        crumbs->setTagsPath(m_context.tagsPath);
        connect(crumbs, &fe::CrumbStrip::crumbActivated, this, &MainWindow::openCrumb);
        connect(crumbs, &fe::CrumbStrip::crumbMenuRequested, this, &MainWindow::showCrumbMenu);
    }
    connect(m_tabBar, &fe::PillTabBar::tabClicked, this, &MainWindow::setCurrentTab);
    connect(m_tabBar, &fe::PillTabBar::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabBar, &fe::PillTabBar::newTabClicked, this, [this] { openTab(homeUrl()); });

    m_actions.inSidebar = [this](const QUrl &folder) { return m_sidebar->view()->showsFolder(folder); };
    m_actions.addToSidebar = [this](const QList<QUrl> &folders) {
        for (const QUrl &folder : folders) {
            m_sidebar->view()->addFolder(folder, fe::placeTitle(folder, m_context));
        }
    };
    m_actions.quickLook = [this] { quickLook(); };
    buildMenuBar();
    qApp->installEventFilter(this);
    m_lastTyped.invalidate();
    openTab(url);
}

fe::TabPage *MainWindow::currentPage() const
{
    return qobject_cast<fe::TabPage *>(m_stack->currentWidget());
}

fe::BrowserTab *MainWindow::currentTab() const
{
    const fe::TabPage *page = currentPage();
    return page ? page->activePane() : nullptr;
}

void MainWindow::openTab(const QUrl &url, bool activate)
{
    auto *tab = new fe::BrowserTab(url);
    auto *page = new fe::TabPage(tab, m_stack);
    const int index = m_stack->addWidget(page);
    m_tabBar->insertTab(index, fe::folderTitle(tab->url(), m_context));
    connectTab(tab);
    // The tab's title and the window follow the pane the user works in.
    connect(page, &fe::TabPage::activePaneChanged, this, [this, page] {
        m_tabBar->setTabTitle(m_stack->indexOf(page), fe::folderTitle(page->activePane()->url(), m_context));
        if (page == currentPage()) {
            syncFromTab();
        }
    });

    if (activate || m_stack->count() == 1) {
        setCurrentTab(index);
    }
}

void MainWindow::connectTab(fe::BrowserTab *tab)
{
    tab->setPicking(m_picking);
    connect(tab, &fe::BrowserTab::filePicked, this, &MainWindow::filePicked);
    connect(tab, &fe::BrowserTab::locationChanged, this, [this, tab] {
        auto *page = qobject_cast<fe::TabPage *>(tab->parentWidget());
        if (page && page->activePane() == tab) {
            m_tabBar->setTabTitle(m_stack->indexOf(page), fe::folderTitle(tab->url(), m_context));
        }
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
                fe::execContextMenu(tab, pos, items, m_actions, this, tab->versionControlActions(groups));
            });
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
    setWindowTitle(fe::folderTitle(tab->url(), m_context));
    // The folder, also while a search shows: the address names where the search started.
    m_toolbar->addressField()->setLocation(tab->folderUrl());
    m_toolbar->addressField()->setCrumbs(fe::crumbsFor(tab->folderUrl(), m_context));
    m_actionBar->setSplit(currentPage()->isSplit());
    m_toolbar->setNavigationState(tab->canGoBack(), tab->canGoForward(), tab->canGoUp());
    m_toolbar->setViewMode(tab->viewMode());
    m_toolbar->setSearchText(tab->searchTerm());
    m_sidebar->view()->setCurrentFolder(tab->folderUrl());
    syncStatus();
    syncEditActions();
}

void MainWindow::syncStatus()
{
    fe::BrowserTab *tab = currentTab();
    if (!tab) {
        return;
    }
    m_pathBar->setStatusText(tab->statusText());
    QList<fe::Crumb> crumbs = fe::crumbsFor(tab->folderUrl(), m_context);
    const KFileItemList selected = tab->selectedItems();
    if (selected.size() == 1 && !tab->isSearching()) {
        crumbs.append({selected.constFirst().text(), selected.constFirst().url()});
        m_pathBar->setCrumbs(crumbs, selected.constFirst().iconName(), selected.constFirst().isDir());
    } else {
        m_pathBar->setCrumbs(crumbs);
    }
    if (m_inspector->isVisible()) {
        m_inspector->setItems(selected, tab->folderUrl());
    }
}

void MainWindow::setInspectorVisible(bool visible)
{
    if (visible) {
        // Filled before it shows, so it opens on the selection and not on what it showed last.
        if (fe::BrowserTab *tab = currentTab()) {
            m_inspector->setItems(tab->selectedItems(), tab->folderUrl());
        }
    }
    m_inspector->setVisible(visible);
    m_inspectorAction->setChecked(visible);
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("file-explorerrc"))->group(QStringLiteral("Inspector"));
    group.writeEntry("Visible", visible);
    group.sync();
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
    m_addToSidebar->setEnabled(!foldersToAddNow().isEmpty());

    const auto partEnabled = [tab](const char *name) {
        const QAction *action = tab->partAction(QString::fromLatin1(name));
        return action && action->isEnabled();
    };
    using fe::FileAction;
    const bool selection = !tab->selectedItems().isEmpty();
    m_actionBar->setActionEnabled(FileAction::NewFolder, partEnabled("create_dir"));
    m_actionBar->setActionEnabled(FileAction::NewFile, partEnabled("create_dir") && tab->newFileMenu());
    m_actionBar->setActionEnabled(FileAction::MoveToTrash, partEnabled("movetotrash"));
    m_actionBar->setActionEnabled(FileAction::Rename, partEnabled("renamefile"));
    m_actionBar->setActionEnabled(FileAction::Duplicate, partEnabled("duplicate"));
    m_actionBar->setActionEnabled(FileAction::QuickLook, selection);
    m_actionBar->setActionEnabled(FileAction::Cut, tab->canCut());
    m_actionBar->setActionEnabled(FileAction::Copy, tab->canCopy());
    m_actionBar->setActionEnabled(FileAction::Paste, tab->canPaste());
    m_actionBar->setActionEnabled(FileAction::OpenTerminal, tab->folderUrl().isLocalFile());
    m_toolbar->tagsButton()->setEnabled(selection);
    m_toolbar->shareButton()->setEnabled(selection);
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
    m_toolbar->addressField()->startEditing();
}

void MainWindow::goToAddress(const QString &text)
{
    fe::BrowserTab *tab = currentTab();
    fe::AddressField *field = m_toolbar->addressField();
    const QUrl target = fe::typedLocation(text, m_context.homePath, tab->folderUrl());
    if (!target.isValid()) {
        field->showError();
        return;
    }
    if (target.isLocalFile()) {
        const QFileInfo info(target.toLocalFile());
        if (!info.exists()) {
            field->showError();
            return;
        }
        if (!info.isDir()) {
            const QUrl folder = QUrl::fromLocalFile(info.absolutePath());
            if (folder.matches(tab->folderUrl(), QUrl::StripTrailingSlash)) {
                tab->selectInPlace({target});
                tab->focusView();
            } else {
                openFolderHere(folder);
                tab->selectItems({target});
            }
            return;
        }
    }
    openFolderHere(target);
}

QList<QUrl> MainWindow::selectedUrls() const
{
    QList<QUrl> urls;
    for (const KFileItem &item : currentTab()->selectedItems()) {
        urls.append(item.targetUrl());
    }
    return urls;
}

void MainWindow::runAction(fe::FileAction action)
{
    fe::BrowserTab *tab = currentTab();
    using fe::FileAction;
    switch (action) {
    case FileAction::NewFolder:
        tab->triggerPartAction(QStringLiteral("create_dir"));
        break;
    case FileAction::NewFile:
        if (KNewFileMenu *newMenu = tab->newFileMenu()) {
            newMenu->setWorkingDirectory(tab->folderUrl());
            newMenu->checkUpToDate();
            const QWidget *button = m_actionBar->button(FileAction::NewFile);
            newMenu->menu()->popup(button->mapToGlobal(QPoint(0, button->height() + 6)));
        }
        break;
    case FileAction::MoveToTrash:
        tab->triggerPartAction(QStringLiteral("movetotrash"));
        break;
    case FileAction::Rename:
        tab->triggerPartAction(QStringLiteral("renamefile"));
        break;
    case FileAction::Duplicate:
        tab->triggerPartAction(QStringLiteral("duplicate"));
        break;
    case FileAction::QuickLook:
        quickLook();
        break;
    case FileAction::Cut:
        tab->cut();
        break;
    case FileAction::Copy:
        tab->copy();
        break;
    case FileAction::Paste:
        tab->paste();
        break;
    case FileAction::OpenTerminal:
        if (tab->folderUrl().isLocalFile()) {
            fe::openTerminalAt(tab->folderUrl().toLocalFile(), this);
        }
        break;
    case FileAction::SplitView:
        toggleSplit();
        break;
    }
}

void MainWindow::toggleSplit()
{
    fe::TabPage *page = currentPage();
    if (page->isSplit()) {
        page->unsplit();
    } else {
        auto *second = new fe::BrowserTab(currentTab()->folderUrl());
        connectTab(second);
        page->split(second);
    }
    syncFromTab();
}

void MainWindow::quickLook()
{
    const KFileItemList selected = currentTab()->selectedItems();
    if (m_quickLook && m_quickLook->isVisible()
        && (selected.isEmpty() || m_quickLook->item().url() == selected.constFirst().url())) {
        m_quickLook->close();
        return;
    }
    if (selected.isEmpty()) {
        return;
    }
    if (!m_quickLook) {
        m_quickLook = new fe::QuickLook(this);
    }
    m_quickLook->showItem(selected.constFirst());
}

void MainWindow::openFolderHere(const QUrl &url)
{
    fe::BrowserTab *tab = currentTab();
    tab->openFolder(url);
    tab->focusView();
}

bool MainWindow::isSelectedFile(const QUrl &url) const
{
    const KFileItemList selected = currentTab()->selectedItems();
    return selected.size() == 1 && !selected.constFirst().isDir() && selected.constFirst().url() == url;
}

void MainWindow::openCrumb(const QUrl &url, fe::CrumbOpen how)
{
    if (isSelectedFile(url)) {
        currentTab()->activateItems(currentTab()->selectedItems());
    } else if (how == fe::CrumbOpen::NewTab) {
        // In the background, as a middle click opens a link in a browser.
        openTab(url, false);
    } else {
        openFolderHere(url);
    }
}

void MainWindow::showCrumbMenu(const QUrl &url, const QPoint &globalPos)
{
    QMenu menu(this);
    if (isSelectedFile(url)) {
        menu.addAction(QIcon::fromTheme(QStringLiteral("document-open")), QStringLiteral("Open"), this,
                       [this, url] { openCrumb(url, fe::CrumbOpen::Here); });
    } else {
        menu.addAction(QIcon::fromTheme(QStringLiteral("document-open-folder")), QStringLiteral("Open"), this,
                       [this, url] { openFolderHere(url); });
        menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), QStringLiteral("Open in New Tab"), this,
                       [this, url] { openTab(url, false); });
        if (url.isLocalFile()) {
            menu.addAction(QIcon::fromTheme(QStringLiteral("utilities-terminal")), QStringLiteral("Open Terminal Here"), this,
                           [this, url] { fe::openTerminalAt(url.toLocalFile(), this); });
        }
    }
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy-path")), QStringLiteral("Copy Path"), this, [url] {
        QGuiApplication::clipboard()->setText(url.toDisplayString(QUrl::PreferLocalFile));
    });
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), QStringLiteral("Get Info"), this,
                   [this, url] { KPropertiesDialog::showDialog(url, this, false); });
    menu.exec(globalPos);
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
    // A window in use keeps the search helper's connection, so a search answers at once and the index follows
    // changes; a window left alone lets it go (fe::SearchService).
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        fe::SearchService::instance().warm();
    }
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
    // Space in a file view opens Quick Look, unless it continues a name being typed to jump to a file.
    if (event->type() == QEvent::KeyPress && watched == QApplication::focusWidget() && currentTab()
        && currentTab()->isAncestorOf(static_cast<QWidget *>(watched)) && !qobject_cast<QTextEdit *>(watched)
        && !qobject_cast<QPlainTextEdit *>(watched) && !qobject_cast<QLineEdit *>(watched)) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Space) {
            const qint64 since = m_lastTyped.isValid() ? m_lastTyped.elapsed() : -1;
            if (fe::spaceShowsQuickLook(key->modifiers(), since) && !currentTab()->selectedItems().isEmpty()) {
                quickLook();
                return true;
            }
        } else if (!key->text().isEmpty() && key->text().at(0).isPrint()) {
            m_lastTyped.restart();
        }
    }
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
        if (auto *page = qobject_cast<fe::TabPage *>(m_stack->widget(i))) {
            for (fe::BrowserTab *tab : page->panes()) {
                tab->setPicking(picking);
            }
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
