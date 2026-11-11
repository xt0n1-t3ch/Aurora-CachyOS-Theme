// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements browsertab.h. All file listing, selection and file operations come from dolphinpart; this file adds
// navigation history, search, status counts and the Finder-style behavior around it.

#include "browsertab.h"

#include "core/paths.h"
#include "filterbar.h"
#include "searchscopebar.h"
#include "searchservice.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KFileItemListProperties>
#include <KFormat>
#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/Global>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KIO/StatJob>
#include <KNewFileMenu>
#include <KParts/FileInfoExtension>
#include <KParts/ListingFilterExtension>
#include <KParts/ListingNotificationExtension>
#include <KParts/OpenUrlArguments>
#include <KParts/PartLoader>
#include <KParts/ReadOnlyPart>
#include <KPluginMetaData>

#include <QFileInfo>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

namespace fe {

namespace {

/// The pause after typing before a search below the folder starts: that search reads the disk, so it waits for a
/// word. A search of the index answers within a frame and starts on every key.
constexpr int SEARCH_DELAY_MS = 300;
/// How long change notices of the index gather before a search on screen lists again, in milliseconds.
constexpr int RELIST_MS = 250;

// The name of the part's action that selects a view mode.
QString viewModeAction(ViewMode mode)
{
    switch (mode) {
    case ViewMode::List:
        return QStringLiteral("details");
    case ViewMode::Compact:
        return QStringLiteral("compact");
    case ViewMode::Icons:
        break;
    }
    return QStringLiteral("icons");
}

} // namespace

BrowserTab::BrowserTab(const QUrl &url, QWidget *parent)
    : QWidget(parent)
    , m_searchTimer(new QTimer(this))
    , m_scopeBar(new SearchScopeBar(this))
    , m_filterBar(new FilterBar(this))
    , m_relistTimer(new QTimer(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_scopeBar);
    m_scopeBar->hide();
    layout->addWidget(m_filterBar);
    // Dolphin's part filters by a case-insensitive part of the name (DolphinPartListingFilterExtension::setFilter and
    // KFileItemModelFilter::matchesPattern, dolphin v26.08.3), the same rule as Dolphin's own filter bar.
    connect(m_filterBar, &FilterBar::filterChanged, this, [this](const QString &text) {
        if (auto *filter = m_part ? KParts::ListingFilterExtension::childObject(m_part) : nullptr) {
            filter->setFilter(KParts::ListingFilterExtension::SubString, text);
        }
        refreshCounts();
    });
    connect(m_filterBar, &FilterBar::focusViewRequested, this, &BrowserTab::focusView);
    m_everywhere = KSharedConfig::openConfig(QStringLiteral("file-explorerrc"))->group(QStringLiteral("Search")).readEntry("Everywhere", true);
    connect(m_scopeBar, &SearchScopeBar::scopeChosen, this, [this](bool everywhere) {
        m_everywhere = everywhere;
        KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("file-explorerrc"))->group(QStringLiteral("Search"));
        group.writeEntry("Everywhere", everywhere);
        group.sync();
        runSearch();
    });
    m_relistTimer->setSingleShot(true);
    m_relistTimer->setInterval(RELIST_MS);
    connect(m_relistTimer, &QTimer::timeout, this, [this] {
        if (isSearching() && m_indexed) {
            reload();
        }
    });
    connect(&SearchService::instance(), &SearchService::indexChanged, this, [this] {
        if (isSearching() && m_indexed) {
            m_relistTimer->start();
        }
    });

    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(SEARCH_DELAY_MS);
    connect(m_searchTimer, &QTimer::timeout, this, &BrowserTab::runSearch);

    const KPluginMetaData metaData = KPluginMetaData::findPluginById(QStringLiteral("kf6/parts"), QStringLiteral("dolphinpart"));
    const auto result = KParts::PartLoader::instantiatePart<KParts::ReadOnlyPart>(metaData, this, this);
    if (!result) {
        auto *error = new QLabel(QStringLiteral("Dolphin's view could not be loaded: %1").arg(result.errorString), this);
        error->setAlignment(Qt::AlignCenter);
        layout->addWidget(error);
        return;
    }
    m_part = result.plugin;
    m_extension = KParts::NavigationExtension::childObject(m_part);
    layout->addWidget(m_part->widget());
    connectPart();
    makeViewTransparent();
    openFolder(url);
}

void BrowserTab::connectPart()
{
    // The part's shortcuts (Delete, F2, Ctrl+A, ...) fire only while the focus is inside this tab,
    // so the tabs that are not shown never react.
    KActionCollection *actions = m_part->actionCollection();
    const auto partActions = actions->actions();
    for (QAction *action : partActions) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }
    actions->addAssociatedWidget(this);
    // The toolbar search field owns Ctrl+F; Dolphin's "Find File…" would start KFind instead.
    if (QAction *find = actions->action(QStringLiteral("edit_find"))) {
        find->setShortcuts({});
    }
    // Finder's name for it; the part says "Duplicate Here".
    if (QAction *duplicate = actions->action(QStringLiteral("duplicate"))) {
        duplicate->setText(QStringLiteral("Duplicate"));
    }

    if (m_extension) {
        connect(m_extension, &KParts::NavigationExtension::openUrlRequest, this, &BrowserTab::handleOpenRequest);
        connect(m_extension, &KParts::NavigationExtension::createNewWindow, this, &BrowserTab::newTabRequested);
        connect(m_extension, &KParts::NavigationExtension::enableAction, this, [this](const char *name, bool enabled) {
            m_enabledActions.insert(QByteArray(name), enabled);
            Q_EMIT editStateChanged();
        });
        connect(m_extension,
                qOverload<const QPoint &, const KFileItemList &, const KParts::OpenUrlArguments &, KParts::NavigationExtension::PopupFlags,
                          const KParts::NavigationExtension::ActionGroupMap &>(&KParts::NavigationExtension::popupMenu),
                this,
                [this](const QPoint &pos, const KFileItemList &items, const KParts::OpenUrlArguments &, KParts::NavigationExtension::PopupFlags,
                       const KParts::NavigationExtension::ActionGroupMap &groups) {
                    Q_EMIT contextMenuRequested(pos, items, groups);
                });
        connect(m_extension, &KParts::NavigationExtension::selectionInfo, this, &BrowserTab::refreshCounts);
        connect(m_extension, &KParts::NavigationExtension::setLocationBarUrl, this, &BrowserTab::locationChanged);
    }
    connect(m_part, &KParts::ReadOnlyPart::completed, this, &BrowserTab::refreshCounts);
    connect(m_part, &KParts::ReadOnlyPart::canceled, this, [this](const QString &message) {
        m_message = message;
        Q_EMIT statusChanged();
    });
    if (auto *listing = KParts::ListingNotificationExtension::childObject(m_part)) {
        connect(listing, &KParts::ListingNotificationExtension::listingEvent, this, &BrowserTab::refreshCounts);
    }
    // Dolphin remembers a view mode per folder, so the mode can change on every navigation.
    for (const ViewMode mode : {ViewMode::Icons, ViewMode::List, ViewMode::Compact}) {
        if (QAction *action = partAction(viewModeAction(mode))) {
            connect(action, &QAction::toggled, this, [this](bool checked) {
                if (checked) {
                    Q_EMIT viewModeChanged();
                }
            });
        }
    }
}

void BrowserTab::makeViewTransparent()
{
    // Dolphin fills its viewport with the color scheme's opaque view background
    // (DolphinView::updatePalette, dolphinview.cpp, v26.08.2, lines 1061-1075). With no widget
    // filling its background, the window glass shows through and only the items are drawn.
    QWidget *view = m_part->widget();
    view->setAutoFillBackground(false);
    const auto children = view->findChildren<QWidget *>();
    for (QWidget *child : children) {
        child->setAutoFillBackground(false);
    }
}

bool BrowserTab::isLoaded() const
{
    return m_part;
}

QUrl BrowserTab::url() const
{
    return m_part ? m_part->url() : QUrl();
}

QUrl BrowserTab::folderUrl() const
{
    return isSearching() ? m_searchOrigin : url();
}

bool BrowserTab::isSearching() const
{
    return !m_searchOrigin.isEmpty();
}

QString BrowserTab::searchTerm() const
{
    return m_searchTerm;
}

void BrowserTab::showUrl(const QUrl &url)
{
    if (!m_part) {
        return;
    }
    m_message.clear();
    m_counts = {};
    // The view keeps its name filter across folders (DolphinView::setNameFilter); a filter typed for one listing would
    // hide items of the next, so it ends here.
    m_filterBar->dismiss();
    m_part->openUrl(url);
    refreshFreeSpace();
    refreshCounts();
    Q_EMIT locationChanged();
}

void BrowserTab::openFolder(const QUrl &url)
{
    endSearch();
    m_history.visit(url);
    showUrl(url);
}

void BrowserTab::endSearch()
{
    m_searchTimer->stop();
    m_relistTimer->stop();
    m_indexed = false;
    m_scopeBar->hide();
    m_searchOrigin.clear();
    m_searchTerm.clear();
}

void BrowserTab::goBack()
{
    if (isSearching()) {
        const QUrl origin = m_searchOrigin;
        endSearch();
        showUrl(origin);
    } else if (m_history.canGoBack()) {
        showUrl(m_history.back());
    }
}

void BrowserTab::goForward()
{
    if (m_history.canGoForward()) {
        endSearch();
        showUrl(m_history.forward());
    }
}

void BrowserTab::goUp()
{
    if (canGoUp()) {
        openFolder(KIO::upUrl(folderUrl()));
    }
}

void BrowserTab::reload()
{
    if (!m_part) {
        return;
    }
    // DolphinPart::openUrl reloads an unchanged URL only when the arguments ask for it.
    KParts::OpenUrlArguments arguments = m_part->arguments();
    arguments.setReload(true);
    m_part->setArguments(arguments);
    m_part->openUrl(url());
    arguments.setReload(false);
    m_part->setArguments(arguments);
    refreshFreeSpace();
}

bool BrowserTab::canGoBack() const
{
    return isSearching() || m_history.canGoBack();
}

bool BrowserTab::canGoForward() const
{
    return m_history.canGoForward();
}

bool BrowserTab::canGoUp() const
{
    const QUrl folder = folderUrl();
    const QUrl up = KIO::upUrl(folder);
    return up.isValid() && !up.matches(folder, QUrl::StripTrailingSlash);
}

void BrowserTab::search(const QString &term)
{
    m_searchTerm = term.trimmed();
    if (!m_searchTerm.isEmpty()) {
        SearchService::instance().warm();
        m_searchTimer->start(m_indexed ? 0 : SEARCH_DELAY_MS);
        return;
    }
    if (isSearching()) {
        const QUrl origin = m_searchOrigin;
        endSearch();
        showUrl(origin);
    } else {
        m_searchTimer->stop();
    }
}

void BrowserTab::runSearch()
{
    if (m_searchTerm.isEmpty() || !m_part) {
        return;
    }
    if (!isSearching()) {
        m_searchOrigin = url();
    }
    const QString title = QStringLiteral("Searching “%1”").arg(m_searchTerm);
    // The index answers for the whole PC or for a folder it holds; anywhere else (a network share, a phone) the
    // folder is searched in place, as before the index existed.
    SearchService &service = SearchService::instance();
    const bool indexAnswers = service.covers(QUrl());
    const bool folderIndexed = indexAnswers && service.covers(m_searchOrigin);
    m_indexed = indexAnswers && (m_everywhere || folderIndexed);
    if (m_indexed) {
        const QString folderName = m_searchOrigin.isLocalFile() ? QFileInfo(m_searchOrigin.toLocalFile()).fileName() : m_searchOrigin.fileName();
        m_scopeBar->setScope(folderName.isEmpty() ? QStringLiteral("/") : folderName, m_everywhere);
        m_scopeBar->setVisible(folderIndexed);
        // Hidden names are matched only while the view shows hidden files, as in the folder itself.
        const QAction *hidden = partAction(QStringLiteral("show_hidden_files"));
        const QString text = hidden && hidden->isChecked() ? m_searchTerm + QStringLiteral(" hidden:") : m_searchTerm;
        showUrl(indexedSearchUrl(text, m_everywhere ? QUrl() : m_searchOrigin, title));
    } else {
        m_scopeBar->hide();
        showUrl(filenameSearchUrl(m_searchTerm, m_searchOrigin, title));
    }
}

void BrowserTab::setViewMode(ViewMode mode)
{
    triggerPartAction(viewModeAction(mode));
}

ViewMode BrowserTab::viewMode() const
{
    for (const ViewMode mode : {ViewMode::List, ViewMode::Compact}) {
        if (const QAction *action = partAction(viewModeAction(mode)); action && action->isChecked()) {
            return mode;
        }
    }
    return ViewMode::Icons;
}

QAction *BrowserTab::partAction(const QString &name) const
{
    return m_part ? m_part->actionCollection()->action(name) : nullptr;
}

void BrowserTab::triggerPartAction(const QString &name) const
{
    if (QAction *action = partAction(name); action && action->isEnabled()) {
        action->trigger();
    }
}

KNewFileMenu *BrowserTab::newFileMenu() const
{
    return m_part ? m_part->findChild<KNewFileMenu *>() : nullptr;
}

KFileItemList BrowserTab::selectedItems() const
{
    auto *info = m_part ? KParts::FileInfoExtension::childObject(m_part) : nullptr;
    return info ? info->queryFor(KParts::FileInfoExtension::SelectedItems) : KFileItemList();
}

bool BrowserTab::canCut() const
{
    return m_enabledActions.value(QByteArrayLiteral("cut"), false);
}

bool BrowserTab::canCopy() const
{
    return m_enabledActions.value(QByteArrayLiteral("copy"), false);
}

bool BrowserTab::canPaste() const
{
    return m_enabledActions.value(QByteArrayLiteral("paste"), false);
}

QString BrowserTab::statusText() const
{
    // KFormat with the desktop's unit setting is what Dolphin's Size column uses, so a file's size
    // reads the same in the column and here.
    const auto formatSize = [](qint64 bytes) { return KFormat().formatByteSize(static_cast<double>(bytes)); };
    return m_message.isEmpty() ? statusLine(m_counts, formatSize) : m_message;
}

void BrowserTab::cut()
{
    if (m_extension && canCut()) {
        QMetaObject::invokeMethod(m_extension.data(), "cut");
    }
}

void BrowserTab::copy()
{
    if (m_extension && canCopy()) {
        QMetaObject::invokeMethod(m_extension.data(), "copy");
    }
}

void BrowserTab::paste()
{
    if (m_extension && canPaste()) {
        QMetaObject::invokeMethod(m_extension.data(), "paste");
    }
}

void BrowserTab::focusView()
{
    if (m_part && m_part->widget()) {
        m_part->widget()->setFocus();
    }
}

void BrowserTab::openFilter()
{
    if (m_part) {
        m_filterBar->open();
    }
}

QString BrowserTab::filterText() const
{
    return m_filterBar->text();
}

FilterBar *BrowserTab::filterBar() const
{
    return m_filterBar;
}

void BrowserTab::activateItems(const KFileItemList &items)
{
    bool openedFolderHere = false;
    for (const KFileItem &item : items) {
        if (item.isDir() && openedFolderHere) {
            Q_EMIT newTabRequested(item.targetUrl());
            continue;
        }
        openedFolderHere = openedFolderHere || item.isDir();
        // The same arguments DolphinPart::slotItemActivated sends (dolphinpart.cpp, v26.08.2, line 333).
        KParts::OpenUrlArguments arguments;
        if (item.targetUrl() == item.url()) {
            arguments.setMimeType(item.mimetype());
        }
        handleOpenRequest(item.targetUrl(), arguments);
    }
}

void BrowserTab::handleOpenRequest(const QUrl &url, const KParts::OpenUrlArguments &arguments)
{
    const QString mimeType = arguments.mimeType();
    if (mimeType.startsWith(u"inode/directory")) {
        openFolder(url);
        return;
    }
    if (m_picking && !(url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir())) {
        Q_EMIT filePicked(url);
        return;
    }
    if (!mimeType.isEmpty()) {
        openExternally(url, mimeType);
        return;
    }
    if (url.isLocalFile()) {
        if (QFileInfo(url.toLocalFile()).isDir()) {
            openFolder(url);
        } else {
            openExternally(url, {});
        }
        return;
    }
    // A link whose target type is unknown yet (network places, desktop links): ask the worker.
    auto *job = KIO::stat(url, KIO::StatJob::SourceSide, KIO::StatBasic, KIO::HideProgressInfo);
    connect(job, &KJob::result, this, [this, job, url] {
        if (!job->error() && job->statResult().isDir()) {
            openFolder(url);
        } else {
            openExternally(url, {});
        }
    });
}

void BrowserTab::openExternally(const QUrl &url, const QString &mimeType)
{
    auto *job = new KIO::OpenUrlJob(url, mimeType);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window()));
    job->setShowOpenOrExecuteDialog(true);
    job->start();
}

void BrowserTab::refreshCounts()
{
    auto *info = m_part ? KParts::FileInfoExtension::childObject(m_part) : nullptr;
    if (!info) {
        return;
    }
    const KFileItemList selected = info->queryFor(KParts::FileInfoExtension::SelectedItems);
    m_counts.items = info->queryFor(KParts::FileInfoExtension::AllItems).size();
    // AllItems holds only what the filter lets through (DolphinPartFileInfoExtension::queryFor, dolphin v26.08.3).
    m_filterBar->setMatchCount(m_counts.items);
    m_counts.selected = selected.size();
    m_counts.selectedFileBytes = 0;
    for (const KFileItem &item : selected) {
        if (!item.isDir()) {
            m_counts.selectedFileBytes += static_cast<qint64>(item.size());
        }
    }
    // Dolphin's main window turns Duplicate on, not its part (dolphinmainwindow.cpp, v26.08.2, line 2665);
    // the same rule here: a selection that can be written, and nothing duplicated inside the Trash.
    if (QAction *duplicate = partAction(QStringLiteral("duplicate"))) {
        duplicate->setEnabled(!selected.isEmpty() && KFileItemListProperties(selected).supportsWriting() && folderUrl().scheme() != u"trash");
    }
    Q_EMIT statusChanged();
}

void BrowserTab::refreshFreeSpace()
{
    m_counts.availableBytes.reset();
    const QUrl folder = folderUrl();
    if (!folder.isLocalFile()) {
        return;
    }
    auto *job = KIO::fileSystemFreeSpace(folder);
    connect(job, &KJob::result, this, [this, job, folder] {
        // A late answer for a folder the tab has already left is dropped.
        if (job->error() || !folderUrl().matches(folder, QUrl::StripTrailingSlash)) {
            return;
        }
        m_counts.availableBytes = static_cast<qint64>(job->availableSize());
        Q_EMIT statusChanged();
    });
}

void BrowserTab::selectItems(const QList<QUrl> &items)
{
    if (m_part) {
        m_part->setProperty("filesToSelect", QVariant::fromValue(items));
    }
}

void BrowserTab::selectInPlace(const QList<QUrl> &items)
{
    // Dolphin selects marked items when a listing completes (DolphinView::updateViewState), and a reload restores the
    // selection it had (dolphinview.cpp, v26.08.2, lines 647-658 and 1763): the mark comes after the reload starts.
    reload();
    selectItems(items);
}

void BrowserTab::setPicking(bool picking)
{
    m_picking = picking;
}

} // namespace fe
