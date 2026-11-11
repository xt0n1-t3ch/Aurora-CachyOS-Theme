// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The widget-free rules of the path segments in the address field and the path bar: where each segment sits, what a
// pointer is over, and what dropping files on a segment does.

#pragma once

#include <QList>
#include <QString>
#include <QUrl>

namespace fe {

/// One place in a row of path segments.
struct CrumbSlot {
    /// The index of the segment this slot shows, or -1 for the ellipsis that stands for the folded segments.
    qsizetype crumb = -1;
    /// The left edge, in pixels.
    qreal x = 0;
    /// The width of the label (icon and text), in pixels, without the chevron after it.
    qreal width = 0;
};

/// A row of path segments laid out in the room a bar has.
struct CrumbLayout {
    QList<CrumbSlot> cells;
    /// The segments folded into the ellipsis, outermost first. Empty when every segment fits.
    QList<qsizetype> folded;
};

/// Lays out segments of natural widths `widths` from `start`, with `separator` pixels between two labels (where the
/// chevron is drawn), in `room` pixels. When they do not fit, the segments between the first and the last fold into
/// one ellipsis `ellipsisWidth` wide, the outermost first, as Finder's path bar does. When even that is too wide, the
/// last segment, the folder on screen, gets what is left (never less than zero) and its label is elided.
[[nodiscard]] CrumbLayout layoutCrumbs(const QList<qreal> &widths, qreal start, qreal separator, qreal ellipsisWidth, qreal room);

/// The part of a segment a pointer is over.
enum class CrumbPart {
    None,
    /// The label, or the ellipsis.
    Label,
    /// The chevron after the label, which lists the folders inside it.
    Chevron,
};

/// What lies at x in a layout.
struct CrumbHit {
    /// The index of the slot in `CrumbLayout::cells`, or -1.
    qsizetype slot = -1;
    CrumbPart part = CrumbPart::None;
};

/// What lies at x: a label, with `slop` pixels of grace on each side, or the chevron between a label and the next one.
/// The last label has no chevron after it.
[[nodiscard]] CrumbHit crumbHitAt(const CrumbLayout &layout, qreal x, qreal separator, qreal slop);

/// What dropping files on a segment does.
enum class CrumbDrop {
    /// Nothing: the drop is refused.
    Refused,
    /// The files move, copy or link into the folder; KIO asks which.
    Transfer,
    /// The files are tagged with the shelf's tag, as a drop on a tag in the sidebar does.
    Tag,
};

/// What dropping `dragged` on the segment `target` does. `targetIsFolder` is false for the selected file the path bar
/// can end with; `tagsPath` is the folder of the tag shelves (empty for none). Refused: no files, a target that is no
/// folder or a virtual place (Recents, a search, Network), a folder dropped on itself or on a folder inside it, files
/// that are all in the target already, the shelf of all tags, and files of another machine dropped on a tag.
[[nodiscard]] CrumbDrop crumbDrop(const QUrl &target, bool targetIsFolder, const QList<QUrl> &dragged, const QString &tagsPath);

} // namespace fe
