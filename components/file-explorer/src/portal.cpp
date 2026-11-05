// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements portal.h. Each call opens a File Explorer window as a panel and waits for the answer in a local event
// loop.

#include "portal.h"

#include "browsertab.h"
#include "chooserbar.h"
#include "mainwindow.h"

#include <KFileItem>
#include <KWindowSystem>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QMessageBox>
#include <QPointer>

QDBusArgument &operator<<(QDBusArgument &arg, const FileChooserPortal::Filter &filter)
{
    arg.beginStructure();
    arg << filter.type << filter.filterString;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, FileChooserPortal::Filter &filter)
{
    arg.beginStructure();
    arg >> filter.type >> filter.filterString;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const FileChooserPortal::FilterList &list)
{
    arg.beginStructure();
    arg << list.userVisibleName << list.filters;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, FileChooserPortal::FilterList &list)
{
    arg.beginStructure();
    arg >> list.userVisibleName >> list.filters;
    arg.endStructure();
    return arg;
}

PortalRequest::PortalRequest(const QDBusObjectPath &handle, QObject *parent)
    : QDBusVirtualObject(parent)
    , m_path(handle.path())
{
    QDBusConnection::sessionBus().registerVirtualObject(m_path, this, QDBusConnection::VirtualObjectRegisterOption::SingleNode);
}

PortalRequest::~PortalRequest()
{
    QDBusConnection::sessionBus().unregisterObject(m_path);
}

bool PortalRequest::handleMessage(const QDBusMessage &message, const QDBusConnection &connection)
{
    if (message.interface() == QLatin1String("org.freedesktop.impl.portal.Request") && message.member() == QLatin1String("Close")) {
        Q_EMIT closeRequested();
        connection.send(message.createReply());
        return true;
    }
    return false;
}

QString PortalRequest::introspect(const QString &) const
{
    return QStringLiteral("<interface name=\"org.freedesktop.impl.portal.Request\"><method name=\"Close\"/></interface>");
}

namespace {

// The response codes of the portal specification.
enum Response : uint { Accepted = 0, Cancelled = 1, Ended = 2 };

// The portal's filters as the panel's file type filters: type 1 entries are MIME types, the others globs.
QList<fe::NameFilter> nameFilters(const FileChooserPortal::FilterListList &lists)
{
    QList<fe::NameFilter> filters;
    for (const FileChooserPortal::FilterList &list : lists) {
        fe::NameFilter filter{list.userVisibleName, {}, {}};
        for (const FileChooserPortal::Filter &entry : list.filters) {
            (entry.type == 1 ? filter.mimeTypes : filter.globs).append(entry.filterString);
        }
        filters.append(filter);
    }
    return filters;
}

// An option value as T; D-Bus sends compound values as a QDBusArgument that needs demarshalling first.
template<typename T>
T fromOption(const QVariantMap &options, const char *key)
{
    const QVariant value = options.value(QLatin1String(key));
    if (value.canConvert<QDBusArgument>()) {
        return qdbus_cast<T>(value.value<QDBusArgument>());
    }
    return value.value<T>();
}

} // namespace

FileChooserPortal::FileChooserPortal(fe::PathContext context, QObject *parent)
    : QDBusAbstractAdaptor(parent)
    , m_context(std::move(context))
{
    qDBusRegisterMetaType<Filter>();
    qDBusRegisterMetaType<Filters>();
    qDBusRegisterMetaType<FilterList>();
    qDBusRegisterMetaType<FilterListList>();
    qDBusRegisterMetaType<QList<QByteArray>>();
}

uint FileChooserPortal::version() const
{
    return 4;
}

void FileChooserPortal::OpenFile(const QDBusObjectPath &handle, const QString &, const QString &parent_window, const QString &title,
                                 const QVariantMap &options, const QDBusMessage &, uint &replyResponse, QVariantMap &replyResults)
{
    run(Mode::Open, handle, parent_window, title, options, replyResponse, replyResults);
}

void FileChooserPortal::SaveFile(const QDBusObjectPath &handle, const QString &, const QString &parent_window, const QString &title,
                                 const QVariantMap &options, const QDBusMessage &, uint &replyResponse, QVariantMap &replyResults)
{
    run(Mode::Save, handle, parent_window, title, options, replyResponse, replyResults);
}

void FileChooserPortal::SaveFiles(const QDBusObjectPath &handle, const QString &, const QString &parent_window, const QString &title,
                                  const QVariantMap &options, const QDBusMessage &, uint &replyResponse, QVariantMap &replyResults)
{
    run(Mode::SaveFiles, handle, parent_window, title, options, replyResponse, replyResults);
}

void FileChooserPortal::run(Mode mode, const QDBusObjectPath &handle, const QString &parentWindow, const QString &title,
                            const QVariantMap &options, uint &replyResponse, QVariantMap &replyResults)
{
    const bool saving = mode != Mode::Open;
    const bool chooseFolders = mode == Mode::SaveFiles || options.value(QStringLiteral("directory")).toBool();
    const bool multiple = options.value(QStringLiteral("multiple")).toBool();
    const FilterListList lists = fromOption<FilterListList>(options, "filters");
    const QList<fe::NameFilter> filters = nameFilters(lists);
    int filterIndex = 0;
    if (options.contains(QStringLiteral("current_filter"))) {
        const FilterList current = fromOption<FilterList>(options, "current_filter");
        for (int i = 0; i < lists.size(); ++i) {
            if (lists[i].userVisibleName == current.userVisibleName) {
                filterIndex = i;
            }
        }
    }

    QString name = options.value(QStringLiteral("current_name")).toString();
    QString folder = fe::portalPath(options.value(QStringLiteral("current_folder")).toByteArray());
    const QString currentFile = fe::portalPath(options.value(QStringLiteral("current_file")).toByteArray());
    if (!currentFile.isEmpty()) {
        const QFileInfo info(currentFile);
        folder = info.absolutePath();
        name = info.fileName();
    }
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
        folder = QDir::homePath();
    }

    QString acceptLabel = options.value(QStringLiteral("accept_label")).toString().remove(QLatin1Char('_'));
    if (acceptLabel.isEmpty()) {
        acceptLabel = saving ? QStringLiteral("Save") : (chooseFolders ? QStringLiteral("Choose") : QStringLiteral("Open"));
    }
    QStringList labels;
    for (const fe::NameFilter &filter : filters) {
        labels.append(filter.label);
    }

    QPointer<MainWindow> window = new MainWindow(QUrl::fromLocalFile(folder), m_context);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle(title.isEmpty() ? acceptLabel : title);
    window->setPicking(true);
    auto *bar = new fe::ChooserBar(mode == Mode::Save, acceptLabel, labels, filterIndex, name, window);
    window->addBottomWidget(bar);

    PortalRequest request(handle);
    QEventLoop loop;
    bool accepted = false;
    bool done = false;
    int chosenFilter = filterIndex;
    QList<QUrl> chosen;

    // The first answer counts: closing the window afterwards (it is destroyed) must not undo an accept.
    const auto finish = [&](bool ok) {
        if (done) {
            return;
        }
        done = true;
        accepted = ok;
        chosenFilter = bar->currentFilter();
        loop.quit();
    };
    const auto accept = [&](const QUrl &picked) {
        fe::BrowserTab *tab = window->visibleTab();
        const QUrl here = tab->folderUrl();
        if (mode == Mode::Save) {
            const fe::NameFilter filter = filters.value(bar->currentFilter());
            const QString fileName = fe::nameForSave(picked.isEmpty() ? bar->name() : picked.fileName(), filter);
            if (fileName.isEmpty()) {
                return;
            }
            const QUrl target = fe::saveTarget(here, fileName);
            if (target.isLocalFile() && QFileInfo::exists(target.toLocalFile())) {
                const auto answer = QMessageBox::warning(window, QString(),
                    QStringLiteral("“%1” already exists. Do you want to replace it?").arg(fileName),
                    QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
                if (answer != QMessageBox::Yes) {
                    return;
                }
            }
            chosen = {target};
            finish(true);
            return;
        }
        if (mode == Mode::SaveFiles) {
            chosen = {here};
            finish(true);
            return;
        }
        QList<QUrl> files;
        QList<QUrl> folders;
        if (!picked.isEmpty()) {
            files.append(picked);
        } else {
            for (const KFileItem &item : tab->selectedItems()) {
                (item.isDir() ? folders : files).append(item.targetUrl());
            }
        }
        // Open with only a folder selected goes into it, as on macOS.
        if (!chooseFolders && files.isEmpty() && folders.size() == 1) {
            tab->openFolder(folders.first());
            return;
        }
        chosen = fe::openResult(files, folders, here, chooseFolders, multiple);
        if (!chosen.isEmpty()) {
            finish(true);
        }
    };

    QObject::connect(bar, &fe::ChooserBar::accepted, &loop, [&] { accept(QUrl()); });
    QObject::connect(bar, &fe::ChooserBar::rejected, &loop, [&] { finish(false); });
    QObject::connect(window, &MainWindow::filePicked, &loop, [&](const QUrl &url) {
        if (mode == Mode::Save) {
            bar->setName(url.fileName());
        } else if (!chooseFolders) {
            accept(url);
        }
    });
    QObject::connect(window, &QObject::destroyed, &loop, [&] { finish(false); });
    QObject::connect(&request, &PortalRequest::closeRequested, &loop, [&] { finish(false); });

    window->show();
    if (!parentWindow.isEmpty()) {
        KWindowSystem::setMainWindow(window->windowHandle(), parentWindow);
    }
    window->raise();
    window->activateWindow();
    loop.exec();
    if (window) {
        window->close();
    }

    replyResponse = accepted ? Accepted : Cancelled;
    if (accepted) {
        QStringList uris;
        if (mode == Mode::SaveFiles) {
            // SaveFiles names the files itself ("files", aay); the panel chose the folder they go in.
            for (const QByteArray &bytes : fromOption<QList<QByteArray>>(options, "files")) {
                uris.append(fe::saveTarget(chosen.first(), fe::portalPath(bytes)).toString());
            }
        } else {
            for (const QUrl &url : std::as_const(chosen)) {
                uris.append(url.toString());
            }
        }
        replyResults.insert(QStringLiteral("uris"), uris);
        if (!lists.isEmpty()) {
            replyResults.insert(QStringLiteral("current_filter"), QVariant::fromValue(lists.value(chosenFilter)));
        }
        replyResults.insert(QStringLiteral("writable"), true);
    }
}
