// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// How folders and other locations are named for the path bar and the window title, and the search URL.

#pragma once

#include <QList>
#include <QString>
#include <QUrl>

namespace fe {

/// One segment of the path bar: what it says and where it leads.
struct Crumb {
    QString label;
    QUrl url;
};

/// Where the user's home is and how the path bar names the start of a path.
struct PathContext {
    QString homePath;
    QString userName;
    QString rootLabel;
    /// The folder of the tag shelves (fe::tagging::shelfRoot); a shelf reads as "Tags", then its tag. Empty for none.
    QString tagsPath = {};
};

/// The path bar segments for a folder, from the outermost place (home or the root disk) to the folder.
[[nodiscard]] QList<Crumb> crumbsFor(const QUrl &url, const PathContext &context);

/// The name the toolbar and the window show for a folder.
[[nodiscard]] QString folderTitle(const QUrl &url, const PathContext &context);

/// A file-name search below a folder, in the form Dolphin's filenamesearch worker reads.
[[nodiscard]] QUrl filenameSearchUrl(const QString &term, const QUrl &folder, const QString &title);

} // namespace fe
