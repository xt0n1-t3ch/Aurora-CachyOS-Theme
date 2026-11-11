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
                                     QStringLiteral("baloosearch"),  QStringLiteral("remote"),
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
    CrumbLayout layout;
    if (widths.isEmpty()) {
        return layout;
    }
    QList<qsizetype> shown;
    for (qsizetype i = 0; i < widths.size(); ++i) {
        shown.append(i);
    }
    // Fold from the second segment on: the first (home or the disk) says where the path starts and the last is the
    // folder on screen, so both stay.
    while (rowWidth(widths, shown, ellipsisWidth, separator) > room && shown.size() > 2) {
        const qsizetype next = shown.at(1) < 0 ? 2 : 1;
        if (next >= shown.size() - 1) {
            break;
        }
        layout.folded.append(shown.at(next));
        if (next == 1) {
            shown[1] = -1;
        } else {
            shown.removeAt(next);
        }
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
