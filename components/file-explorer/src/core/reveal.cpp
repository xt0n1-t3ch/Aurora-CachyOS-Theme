// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements reveal.h.

#include "core/reveal.h"

namespace fe {

namespace {

/// A file:// URI or a plain path, without a trailing slash.
QUrl normalized(const QString &uri)
{
    return QUrl::fromUserInput(uri, QString(), QUrl::AssumeLocalFile).adjusted(QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
}

} // namespace

QList<Reveal> revealGroups(const QStringList &uris)
{
    QList<Reveal> groups;
    for (const QString &uri : uris) {
        const QUrl item = normalized(uri);
        const QUrl folder = item.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
        auto group = std::find_if(groups.begin(), groups.end(), [&folder](const Reveal &r) {
            return r.folder == folder;
        });
        if (group == groups.end()) {
            groups.append(Reveal{folder, {item}});
        } else if (!group->items.contains(item)) {
            group->items.append(item);
        }
    }
    return groups;
}

QList<QUrl> foldersToShow(const QStringList &uris)
{
    QList<QUrl> folders;
    for (const QString &uri : uris) {
        const QUrl folder = normalized(uri);
        if (!folders.contains(folder)) {
            folders.append(folder);
        }
    }
    return folders;
}

} // namespace fe
