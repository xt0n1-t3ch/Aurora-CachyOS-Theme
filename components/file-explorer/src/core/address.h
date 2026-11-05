// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The address field of the toolbar: what it shows for a location, where a typed path leads, and how it completes.

#pragma once

#include <QStringList>
#include <QUrl>

namespace fe {

/// What the address field shows: a local folder's path, the folder a search looks in, or any other place's URL.
[[nodiscard]] QString addressText(const QUrl &url);

/// Where a typed address leads. "~" starts at homePath; a path with neither "/" nor a scheme in front starts at
/// currentFolder; "." and ".." are resolved; a URL with a scheme (smb://, trash:/) is taken as written. Blank text
/// leads nowhere (an invalid URL).
[[nodiscard]] QUrl typedLocation(const QString &text, const QString &homePath, const QUrl &currentFolder);

/// The text up to the last "/" (the folder being typed into) and the start of the name after it.
struct TypedName {
    QString folderText;
    QString stem;
};
[[nodiscard]] TypedName splitTyped(const QString &text);

/// The completions of text, given the names of the subfolders of its folder (splitTyped): folderText + name + "/"
/// for every name that starts with the stem, ignoring case. Hidden folders come only after a typed ".". Sorted the
/// way people count (2 before 10), at most limit.
[[nodiscard]] QStringList addressSuggestions(const QString &text, const QStringList &folderNames, int limit);

/// Tab, as in a shell: text grows to the longest start all its completions share, and the only completion is taken
/// whole with its "/". Without a completion text stays as it is.
[[nodiscard]] QString tabCompleted(const QString &text, const QStringList &folderNames);

} // namespace fe
