// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements places.h.

#include "core/places.h"

namespace fe {

namespace {

using Group = QList<SidebarMenuEntry>;

bool isSystemDisk(const QUrl &url)
{
    return url.isLocalFile() && url.toLocalFile() == u"/";
}

} // namespace

QList<QList<SidebarMenuEntry>> sidebarMenu(const SidebarPlace &place)
{
    QList<Group> groups{Group{{SidebarCommand::Open}, {SidebarCommand::OpenInNewTab}}};
    Group info;
    if (place.url.isLocalFile()) {
        info.append(SidebarMenuEntry{SidebarCommand::GetInfo});
    }
    if (place.renamable) {
        info.append(SidebarMenuEntry{SidebarCommand::Rename});
    }
    if (!info.isEmpty()) {
        groups.append(info);
    }

    Group last;
    if (place.ejectAllowed) {
        last.append(SidebarMenuEntry{SidebarCommand::Eject});
    } else if (place.teardownAllowed && !isSystemDisk(place.url)) {
        // Finder ejects a drive that can be pulled out; a built-in one is only unmounted.
        last.append(SidebarMenuEntry{place.fixedDrive ? SidebarCommand::Unmount : SidebarCommand::Eject});
    }
    if (place.addedByUser) {
        last.append(SidebarMenuEntry{SidebarCommand::RemoveFromSidebar});
    }
    if (place.url.scheme() == u"trash") {
        last.append(SidebarMenuEntry{SidebarCommand::EmptyTrash, !place.trashEmpty});
    }
    if (!last.isEmpty()) {
        groups.append(last);
    }
    return groups;
}

QString sidebarCommandText(SidebarCommand command, const QString &title)
{
    switch (command) {
    case SidebarCommand::Open:
        return QStringLiteral("Open");
    case SidebarCommand::OpenInNewTab:
        return QStringLiteral("Open in New Tab");
    case SidebarCommand::GetInfo:
        return QStringLiteral("Get Info");
    case SidebarCommand::Rename:
        // Finder renames in place, so no ellipsis (that would promise a dialog).
        return QStringLiteral("Rename “%1”").arg(title);
    case SidebarCommand::Eject:
        return QStringLiteral("Eject “%1”").arg(title);
    case SidebarCommand::Unmount:
        return QStringLiteral("Unmount “%1”").arg(title);
    case SidebarCommand::RemoveFromSidebar:
        return QStringLiteral("Remove from Sidebar");
    case SidebarCommand::EmptyTrash:
        return QStringLiteral("Empty Trash");
    }
    return {};
}

QList<QUrl> foldersToAdd(const QList<QUrl> &selectedFolders, const QUrl &currentFolder, const std::function<bool(const QUrl &)> &inSidebar)
{
    const QList<QUrl> asked = selectedFolders.isEmpty() ? QList<QUrl>{currentFolder} : selectedFolders;
    QList<QUrl> folders;
    for (const QUrl &url : asked) {
        const QUrl folder = url.adjusted(QUrl::StripTrailingSlash);
        if (!folder.isEmpty() && !folders.contains(folder) && !inSidebar(folder)) {
            folders.append(folder);
        }
    }
    return folders;
}

SidebarDrop sidebarRowDrop(qreal y, qreal height, bool sameSection)
{
    if (!sameSection) {
        return SidebarDrop::Nowhere;
    }
    return y < height / 2 ? SidebarDrop::BeforeRow : SidebarDrop::AfterRow;
}

QList<qsizetype> sidebarOrder(const QStringList &keys, const QStringList &saved)
{
    QList<qsizetype> order;
    order.reserve(keys.size());
    for (const QString &key : saved) {
        const qsizetype at = keys.indexOf(key);
        if (at >= 0 && !order.contains(at)) {
            order.append(at);
        }
    }
    for (qsizetype at = 0; at < keys.size(); ++at) {
        if (!order.contains(at)) {
            order.append(at);
        }
    }
    return order;
}

QStringList movedSidebarRow(QStringList keys, qsizetype from, qsizetype to, bool below)
{
    if (from == to || from < 0 || to < 0 || from >= keys.size() || to >= keys.size()) {
        return keys;
    }
    const QString target = keys.at(to);
    const QString moving = keys.takeAt(from);
    keys.insert(keys.indexOf(target) + (below ? 1 : 0), moving);
    return keys;
}

SidebarDrop sidebarDrop(qreal y, qreal height, bool inFavorites, bool acceptsFiles, bool onlyFolders)
{
    if (inFavorites && onlyFolders) {
        // A row's edge quarters insert; a row that takes no files inserts from either half.
        const qreal band = acceptsFiles ? height / 4 : height / 2;
        if (y < band) {
            return SidebarDrop::BeforeRow;
        }
        if (y >= height - band) {
            return SidebarDrop::AfterRow;
        }
    }
    return acceptsFiles ? SidebarDrop::IntoPlace : SidebarDrop::Nowhere;
}

QString placeTitle(const QUrl &url, const PathContext &context)
{
    if (url.isLocalFile()) {
        return folderTitle(url, context);
    }
    const QString name = url.adjusted(QUrl::StripTrailingSlash).fileName();
    return name.isEmpty() ? url.host() : name;
}

} // namespace fe
