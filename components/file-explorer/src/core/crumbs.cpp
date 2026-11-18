// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements crumbs.h.

#include "core/crumbs.h"

#include <QDir>

#include <algorithm>

namespace fe {

namespace {

/// Places KIO lists but that hold no files of their own, so nothing can be dropped into them.
bool isVirtualPlace(const QUrl &url)
{
    static const QStringList schemes{QStringLiteral("recentlyused"), QStringLiteral("filenamesearch"),
                                     QStringLiteral("baloosearch"),  QStringLiteral("aurora-search"), QStringLiteral("remote"),
                                     QStringLiteral("network"),      QStringLiteral("timeline"),
                                     QStringLiteral("tags"),         QStringLiteral("applications"),
                                     QStringLiteral("settings")};
    return schemes.contains(url.scheme());
}

QUrl normalized(const QUrl &url)
{
    return url.adjusted(QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
}

QUrl parentOf(const QUrl &url)
{
    return normalized(url).adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
}

qreal rowWidth(const QList<qreal> &widths, const QList<qsizetype> &shown, qreal ellipsisWidth, qreal separator)
{
    qreal total = 0;
    for (const qsizetype index : shown) {
        total += index < 0 ? ellipsisWidth : widths.at(index);
    }
    return total + separator * qreal(std::max<qsizetype>(shown.size() - 1, 0));
}

} // namespace

CrumbLayout layoutCrumbs(const QList<qreal> &widths, qreal start, qreal separator, qreal ellipsisWidth, qreal room)
{
    // Enough room for a long folder name at the end to keep its start and end legible around an ellipsis before its
    // parent folders are folded away for it.
    constexpr qreal CURRENT_FOLDER_FLOOR = 110.0;

    CrumbLayout layout;
    if (widths.isEmpty()) {
        return layout;
    }
    QList<qsizetype> shown;
    shown.reserve(widths.size());
    for (qsizetype i = 0; i < widths.size(); ++i) {
        shown.append(i);
    }
    QList<qreal> fitWidths = widths;
    fitWidths.last() = std::min(fitWidths.last(), CURRENT_FOLDER_FLOOR);

    // Fold from the outermost segment inward, as Windows' address bar does, so the enclosing folders right above the
    // current one stay visible until no room is left for them.
    while (rowWidth(fitWidths, shown, ellipsisWidth, separator) > room && shown.size() > 1) {
        const qsizetype next = shown.at(0) < 0 ? 1 : 0;
        if (next >= shown.size() - 1) {
            break;
        }
        layout.folded.append(shown.at(next));
        if (next == 0) {
            shown[0] = -1;
        } else {
            shown.removeAt(next);
        }
    }
    if (layout.folded.size() == 1 && widths.at(layout.folded.constFirst()) <= ellipsisWidth) {
        shown[0] = layout.folded.takeFirst();
    }

    qreal x = start;
    for (const qsizetype index : std::as_const(shown)) {
        const qreal width = index < 0 ? ellipsisWidth : widths.at(index);
        layout.cells.append({index, x, width});
        x += width + separator;
    }
    CrumbSlot &last = layout.cells.last();
    last.width = std::clamp(start + room - last.x, 0.0, last.width);
    return layout;
}

CrumbHit crumbHitAt(const CrumbLayout &layout, qreal x, qreal separator, qreal slop)
{
    for (qsizetype i = 0; i < layout.cells.size(); ++i) {
        const CrumbSlot &slot = layout.cells.at(i);
        if (x >= slot.x - slop && x <= slot.x + slot.width + slop) {
            return {i, CrumbPart::Label};
        }
        const bool last = i == layout.cells.size() - 1;
        if (!last && x > slot.x + slot.width + slop && x < slot.x + slot.width + separator - slop) {
            return {i, CrumbPart::Chevron};
        }
    }
    return {};
}

CrumbDrop crumbDrop(const QUrl &target, bool targetIsFolder, const QList<QUrl> &dragged, const QString &tagsPath)
{
    if (dragged.isEmpty() || !target.isValid() || target.isEmpty() || !targetIsFolder || isVirtualPlace(target)) {
        return CrumbDrop::Refused;
    }
    const QUrl into = normalized(target);

    if (!tagsPath.isEmpty() && into.isLocalFile()) {
        const QString path = QDir::cleanPath(into.toLocalFile());
        const QString tags = QDir::cleanPath(tagsPath);
        if (path == tags) {
            return CrumbDrop::Refused;
        }
        if (QDir::cleanPath(path + QStringLiteral("/..")) == tags) {
            // A shelf holds links that File Explorer keeps in step with the tags; files dropped there get the tag.
            const bool allLocal = std::all_of(dragged.cbegin(), dragged.cend(), [](const QUrl &url) { return url.isLocalFile(); });
            return allLocal ? CrumbDrop::Tag : CrumbDrop::Refused;
        }
    }

    bool allThereAlready = true;
    for (const QUrl &url : dragged) {
        const QUrl item = normalized(url);
        if (item == into || item.isParentOf(into)) {
            return CrumbDrop::Refused;
        }
        allThereAlready = allThereAlready && parentOf(item) == into;
    }
    return allThereAlready ? CrumbDrop::Refused : CrumbDrop::Transfer;
}

} // namespace fe
