// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements paths.h.

#include "core/paths.h"

#include <QDir>
#include <QUrlQuery>

namespace fe {

namespace {

/// The single path bar segment for a location that is not a local folder.
QString remoteTitle(const QUrl &url)
{
    const QString scheme = url.scheme();
    if (scheme == u"trash") {
        return QStringLiteral("Trash");
    }
    if (scheme == u"recentlyused") {
        return QStringLiteral("Recents");
    }
    if (scheme == u"filenamesearch" || scheme == u"baloosearch" || scheme == u"aurora-search") {
        const QString title = QUrlQuery(url).queryItemValue(QStringLiteral("title"), QUrl::FullyDecoded);
        return title.isEmpty() ? QStringLiteral("Search") : title;
    }
    if (scheme == u"remote" || scheme == u"network") {
        return QStringLiteral("Network");
    }
    if (!url.host().isEmpty()) {
        return url.host();
    }
    return url.toDisplayString(QUrl::PreferLocalFile);
}

} // namespace

QList<Crumb> crumbsFor(const QUrl &url, const PathContext &context)
{
    if (!url.isLocalFile()) {
        return {{remoteTitle(url), url}};
    }

    const QString path = QDir::cleanPath(url.toLocalFile());
    const QString home = QDir::cleanPath(context.homePath);

    if (!context.tagsPath.isEmpty()) {
        const QString tags = QDir::cleanPath(context.tagsPath);
        if (path.startsWith(tags + u'/')) {
            // A tagged folder opened from its shelf goes on under the tag.
            QList<Crumb> crumbs{{QStringLiteral("Tags"), QUrl::fromLocalFile(tags)}};
            QString base = tags;
            for (const QString &part : path.mid(tags.size() + 1).split(u'/', Qt::SkipEmptyParts)) {
                base += u'/' + part;
                crumbs.append({part, QUrl::fromLocalFile(base)});
            }
            return crumbs;
        }
    }

    QList<Crumb> crumbs;
    QString base;
    QString rest;
    if (path == home || path.startsWith(home + u'/')) {
        crumbs.append({context.userName, QUrl::fromLocalFile(home)});
        base = home;
        rest = path.mid(home.size());
    } else {
        crumbs.append({context.rootLabel, QUrl::fromLocalFile(QStringLiteral("/"))});
        rest = path;
    }

    const QStringList parts = rest.split(u'/', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        base += u'/' + part;
        crumbs.append({part, QUrl::fromLocalFile(base)});
    }
    return crumbs;
}

QString folderTitle(const QUrl &url, const PathContext &context)
{
    return crumbsFor(url, context).constLast().label;
}

QUrl indexedSearchUrl(const QString &term, const QUrl &scope, const QString &title)
{
    // Encoded as filenameSearchUrl encodes, so a term with & or = survives.
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), QString::fromLatin1(QUrl::toPercentEncoding(term)));
    if (scope.isLocalFile()) {
        query.addQueryItem(QStringLiteral("scope"), QString::fromLatin1(QUrl::toPercentEncoding(scope.toLocalFile())));
    }
    query.addQueryItem(QStringLiteral("title"), QString::fromLatin1(QUrl::toPercentEncoding(title)));
    QUrl url;
    url.setScheme(QStringLiteral("aurora-search"));
    url.setPath(QStringLiteral("/"));
    url.setQuery(query);
    return url;
}

QUrl filenameSearchUrl(const QString &term, const QUrl &folder, const QString &title)
{
    // The same query items, in the same encoding, as Dolphin's DolphinQuery::toUrl
    // (src/search/dolphinquery.cpp, v26.08.2, lines 232-248).
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("search"), QString::fromLatin1(QUrl::toPercentEncoding(term)));
    query.addQueryItem(QStringLiteral("url"), QString::fromLatin1(QUrl::toPercentEncoding(folder.url())));
    query.addQueryItem(QStringLiteral("title"), QString::fromLatin1(QUrl::toPercentEncoding(title)));

    QUrl url;
    url.setScheme(QStringLiteral("filenamesearch"));
    url.setQuery(query);
    return url;
}

} // namespace fe
