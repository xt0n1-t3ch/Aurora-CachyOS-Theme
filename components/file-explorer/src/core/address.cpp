// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements address.h.

#include "core/address.h"

#include <QCollator>
#include <QDir>
#include <QLocale>
#include <QRegularExpression>
#include <QUrlQuery>

#include <algorithm>

namespace fe {

namespace {

/// A scheme followed by ":" at the start, as in "smb://nas" or "trash:/".
bool hasScheme(const QString &text)
{
    static const QRegularExpression SCHEME(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:"));
    return SCHEME.match(text).hasMatch();
}

/// The names that complete stem, ignoring case, sorted the way people count.
QStringList matchingNames(const QString &stem, const QStringList &names)
{
    const bool hidden = stem.startsWith(u'.');
    QStringList matches;
    for (const QString &name : names) {
        if ((hidden || !name.startsWith(u'.')) && name.startsWith(stem, Qt::CaseInsensitive)) {
            matches.append(name);
        }
    }
    QCollator collator{QLocale(QLocale::English)};
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(matches.begin(), matches.end(), collator);
    return matches;
}

} // namespace

QString addressText(const QUrl &url)
{
    if (url.scheme() == u"filenamesearch") {
        // The folder searched, as fe::filenameSearchUrl stores it.
        return addressText(QUrl(QUrlQuery(url).queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded)));
    }
    if (url.isLocalFile()) {
        const QString path = QDir::cleanPath(url.toLocalFile());
        return path.isEmpty() ? QStringLiteral("/") : path;
    }
    return url.toDisplayString();
}

QUrl typedLocation(const QString &text, const QString &homePath, const QUrl &currentFolder)
{
    QString path = text.trimmed();
    if (path.isEmpty()) {
        return {};
    }
    if (hasScheme(path)) {
        const QUrl url(path);
        return url.isLocalFile() ? QUrl::fromLocalFile(QDir::cleanPath(url.toLocalFile())) : url;
    }
    if (path == u"~" || path.startsWith(QStringLiteral("~/"))) {
        path = homePath + path.mid(1);
    } else if (!path.startsWith(u'/')) {
        if (!currentFolder.isLocalFile()) {
            return {};
        }
        path = currentFolder.toLocalFile() + u'/' + path;
    }
    return QUrl::fromLocalFile(QDir::cleanPath(path));
}

TypedName splitTyped(const QString &text)
{
    const qsizetype slash = text.lastIndexOf(u'/');
    return {text.left(slash + 1), text.mid(slash + 1)};
}

QStringList addressSuggestions(const QString &text, const QStringList &folderNames, int limit)
{
    const TypedName typed = splitTyped(text);
    QStringList suggestions;
    for (const QString &name : matchingNames(typed.stem, folderNames)) {
        if (suggestions.size() >= limit) {
            break;
        }
        suggestions.append(typed.folderText + name + u'/');
    }
    return suggestions;
}

QString tabCompleted(const QString &text, const QStringList &folderNames)
{
    const TypedName typed = splitTyped(text);
    const QStringList matches = matchingNames(typed.stem, folderNames);
    if (matches.isEmpty()) {
        return text;
    }
    if (matches.size() == 1) {
        return typed.folderText + matches.constFirst() + u'/';
    }
    // The longest start, ignoring case, that every match shares; spelled like the first match.
    const QString &first = matches.constFirst();
    qsizetype shared = first.size();
    for (const QString &match : matches) {
        qsizetype i = 0;
        while (i < shared && i < match.size() && first.at(i).toCaseFolded() == match.at(i).toCaseFolded()) {
            ++i;
        }
        shared = i;
    }
    // Never shorter than the stem, since every match starts with it; the stem takes the matches' spelling.
    return typed.folderText + first.left(shared);
}

} // namespace fe
