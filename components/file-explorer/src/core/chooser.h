// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The pure logic behind the Open and Save panels: file type filters, save names and the answer to the portal.

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace fe {

/// One entry of an Open or Save panel's file type popup, from a portal filter: globs ("*.png") and MIME types.
struct NameFilter {
    QString label;
    QStringList globs;
    QStringList mimeTypes;
};

/// Whether a file of this name and MIME type shows under the filter; an empty filter shows everything.
[[nodiscard]] bool filterAccepts(const NameFilter &filter, const QString &fileName, const QString &mimeType);

/// The space-separated wildcard list Dolphin's name filter takes; MIME types become their globs.
[[nodiscard]] QString viewPattern(const NameFilter &filter, const QStringList &mimeGlobs);

/// The name a Save panel writes: the typed name, given the extension of a one-glob filter ("*.png") when it has none.
[[nodiscard]] QString nameForSave(const QString &typed, const NameFilter &filter);

/// Where a Save panel writes the name in a folder.
[[nodiscard]] QUrl saveTarget(const QUrl &folder, const QString &name);

/// What an Open panel answers: the selected files (or folders, when choosing folders), the first one only unless
/// several may be chosen; choosing folders with nothing selected answers the folder on screen.
[[nodiscard]] QList<QUrl> openResult(const QList<QUrl> &selectedFiles, const QList<QUrl> &selectedFolders,
                                     const QUrl &currentFolder, bool chooseFolders, bool multiple);

/// A portal path option ("current_folder", "current_file"): bytes ending in a NUL.
[[nodiscard]] QString portalPath(const QByteArray &bytes);

} // namespace fe
