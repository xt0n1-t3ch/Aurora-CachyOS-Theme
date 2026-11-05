// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Finder's color tags: the seven colors, adding and removing a tag, the emblem a tagged file shows, and the names
// in the shelf folder that lists a tag's files. Tags live in the file's user.xdg.tags attribute, which Dolphin and
// Baloo read too.

#pragma once

#include <QColor>
#include <QList>
#include <QStringList>

#include <optional>

namespace fe {

/// One of Finder's color tags.
struct ColorTag {
    QString name;
    QColor color;
};

/// Red, Orange, Yellow, Green, Blue, Purple and Gray, in Finder's order, in macOS's dark system colors.
[[nodiscard]] const QList<ColorTag> &colorTags();

/// The color tag called name, ignoring case.
[[nodiscard]] std::optional<ColorTag> colorTag(const QString &name);

/// tags with tag added at the end (on) or removed (off). Tags compare ignoring case; a tag is never there twice.
[[nodiscard]] QStringList taggedWith(const QStringList &tags, const QString &tag, bool on);

/// The color tags among tags, by their own names, in Finder's order.
[[nodiscard]] QStringList colorTagNames(const QStringList &tags);

/// The emblem icon of a file with these tags: its first three colors as dots ("emblem-aurora-tags-red-blue"), or
/// nothing without a color tag.
[[nodiscard]] QString tagEmblem(const QStringList &tags);

/// Every emblem tagEmblem can name: the icon theme ships them all.
[[nodiscard]] QStringList tagEmblemNames();

/// The name of a file's link in a tag's shelf folder: its own name, or "name 2.ext", "name 3.ext" while taken.
[[nodiscard]] QString shelfEntryName(const QString &fileName, const QStringList &taken);

} // namespace fe
