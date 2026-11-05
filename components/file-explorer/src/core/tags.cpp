// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements tags.h.

#include "core/tags.h"

namespace fe {

namespace {

/// The most dots an emblem shows, like Finder's overlapping tag dots.
constexpr int EMBLEM_DOTS = 3;

QString emblemName(const QStringList &colors)
{
    return QStringLiteral("emblem-aurora-tags-") + colors.join(u'-').toLower();
}

} // namespace

const QList<ColorTag> &colorTags()
{
    static const QList<ColorTag> TAGS{
        {QStringLiteral("Red"), QColor(255, 69, 58)},     {QStringLiteral("Orange"), QColor(255, 159, 10)},
        {QStringLiteral("Yellow"), QColor(255, 214, 10)}, {QStringLiteral("Green"), QColor(48, 209, 88)},
        {QStringLiteral("Blue"), QColor(10, 132, 255)},   {QStringLiteral("Purple"), QColor(191, 90, 242)},
        {QStringLiteral("Gray"), QColor(152, 152, 157)},
    };
    return TAGS;
}

std::optional<ColorTag> colorTag(const QString &name)
{
    for (const ColorTag &tag : colorTags()) {
        if (tag.name.compare(name, Qt::CaseInsensitive) == 0) {
            return tag;
        }
    }
    return std::nullopt;
}

QStringList taggedWith(const QStringList &tags, const QString &tag, bool on)
{
    QStringList result;
    for (const QString &existing : tags) {
        if (existing.compare(tag, Qt::CaseInsensitive) != 0) {
            result.append(existing);
        } else if (on) {
            return tags;
        }
    }
    if (on) {
        result.append(tag);
    }
    return result;
}

QStringList colorTagNames(const QStringList &tags)
{
    QStringList colors;
    for (const ColorTag &color : colorTags()) {
        if (tags.contains(color.name, Qt::CaseInsensitive)) {
            colors.append(color.name);
        }
    }
    return colors;
}

QString tagEmblem(const QStringList &tags)
{
    const QStringList colors = colorTagNames(tags);
    return colors.isEmpty() ? QString() : emblemName(colors.mid(0, EMBLEM_DOTS));
}

QStringList tagEmblemNames()
{
    // Every set of one to three colors, each in Finder's order.
    QStringList names;
    const QList<ColorTag> &all = colorTags();
    const auto count = all.size();
    for (qsizetype mask = 1; mask < (qsizetype(1) << count); ++mask) {
        QStringList colors;
        for (qsizetype i = 0; i < count; ++i) {
            if (mask & (qsizetype(1) << i)) {
                colors.append(all.at(i).name);
            }
        }
        if (colors.size() <= EMBLEM_DOTS) {
            names.append(emblemName(colors));
        }
    }
    return names;
}

QString shelfEntryName(const QString &fileName, const QStringList &taken)
{
    if (!taken.contains(fileName)) {
        return fileName;
    }
    // A dot at the start makes a hidden name, not an extension.
    const qsizetype dot = fileName.lastIndexOf(u'.');
    const bool hasExtension = dot > 0;
    const QString base = hasExtension ? fileName.left(dot) : fileName;
    const QString extension = hasExtension ? fileName.mid(dot) : QString();
    for (int n = 2;; ++n) {
        const QString candidate = QStringLiteral("%1 %2%3").arg(base).arg(n).arg(extension);
        if (!taken.contains(candidate)) {
            return candidate;
        }
    }
}

} // namespace fe
