// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Color tags on real files: reading and writing user.xdg.tags, the shelf folders the sidebar's Tags open, and the
// menu entries that tag the selection.

#pragma once

#include <QList>
#include <QStringList>
#include <QUrl>

class QMenu;

namespace fe::tagging {

/// The tags of a local file; nothing for any other place.
[[nodiscard]] QStringList tagsOf(const QUrl &url);

/// Whether every one of items carries tag; false without items.
[[nodiscard]] bool allTagged(const QList<QUrl> &items, const QString &tag);

/// Adds (on) or removes tag on every local item, keeps the tag's shelf in step, and tells file views to show it.
void setTag(const QList<QUrl> &items, const QString &tag, bool on);

/// The folder that holds a link to every file tagged tag, created when missing and cleared of links to files that
/// are gone or lost the tag. It lives in the user's data folder (file-explorer/tags/<tag>).
[[nodiscard]] QUrl shelfUrl(const QString &tag);

/// The folder of all the shelves, for the path bar to name.
[[nodiscard]] QString shelfRoot();

/// Adds to menu the row of color dots and a Tags submenu, both tagging items; a dot is filled when every item
/// carries its tag.
void addTagEntries(QMenu &menu, const QList<QUrl> &items);

} // namespace fe::tagging
