// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements chooser.h. Free of widgets so the panel rules can be tested on their own.

#include "core/chooser.h"

#include <QRegularExpression>

namespace fe {

bool filterAccepts(const NameFilter &filter, const QString &fileName, const QString &mimeType)
{
    if (filter.globs.isEmpty() && filter.mimeTypes.isEmpty()) {
        return true;
    }
    for (const QString &glob : filter.globs) {
        const QRegularExpression wildcard(QRegularExpression::wildcardToRegularExpression(glob), QRegularExpression::CaseInsensitiveOption);
        if (wildcard.match(fileName).hasMatch()) {
            return true;
        }
    }
    return filter.mimeTypes.contains(mimeType);
}

QString viewPattern(const NameFilter &filter, const QStringList &mimeGlobs)
{
    QStringList patterns = filter.globs;
    for (const QString &glob : mimeGlobs) {
        if (!patterns.contains(glob)) {
            patterns.append(glob);
        }
    }
    return patterns.join(QLatin1Char(' '));
}

QString nameForSave(const QString &typed, const NameFilter &filter)
{
    const QString name = typed.trimmed();
    static const QRegularExpression extensionGlob(QStringLiteral("^\\*(\\.[^*?\\[\\]]+)$"));
    if (filter.globs.size() == 1 && !name.isEmpty() && !name.contains(QLatin1Char('.'))) {
        const QRegularExpressionMatch match = extensionGlob.match(filter.globs.first());
        if (match.hasMatch()) {
            return name + match.captured(1);
        }
    }
    return name;
}

QUrl saveTarget(const QUrl &folder, const QString &name)
{
    QUrl target = folder.adjusted(QUrl::StripTrailingSlash);
    target.setPath(target.path() + QLatin1Char('/') + name);
    return target;
}

QList<QUrl> openResult(const QList<QUrl> &selectedFiles, const QList<QUrl> &selectedFolders, const QUrl &currentFolder,
                       bool chooseFolders, bool multiple)
{
    QList<QUrl> chosen = chooseFolders ? selectedFolders : selectedFiles;
    if (chosen.isEmpty() && chooseFolders) {
        chosen.append(currentFolder);
    }
    if (!multiple && chosen.size() > 1) {
        chosen = chosen.mid(0, 1);
    }
    return chosen;
}

QString portalPath(const QByteArray &bytes)
{
    QByteArray path = bytes;
    while (path.endsWith('\0')) {
        path.chop(1);
    }
    return QString::fromUtf8(path);
}

} // namespace fe
