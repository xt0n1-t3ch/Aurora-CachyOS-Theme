// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The geometry of the title bar: where the window buttons and the caption go. It uses no KWin types.

#pragma once

#include <QList>
#include <QRectF>

namespace aurora {

// The title bar measures in pixels, matched to the File Explorer's toolbar capsule.
inline constexpr qreal TITLE_BAR_HEIGHT = 46;
inline constexpr qreal CAPSULE_HEIGHT = 34;
inline constexpr qreal SEGMENT_WIDTH = 38;
// Space between the capsule's end and its first or last segment.
inline constexpr qreal CAPSULE_PADDING = 3;
// Space between the capsule and the window's right edge.
inline constexpr qreal SIDE_MARGIN = 8;
// Least space between the caption and the window's left edge or the capsule.
inline constexpr qreal CAPTION_GAP = 12;

// Where the window buttons are drawn and where they take clicks.
struct TitleLayout {
    QRectF capsule;          // The glass capsule behind the buttons; empty without buttons.
    QList<QRectF> buttons;   // Each button's segment of the capsule, left to right.
    QList<QRectF> hitAreas;  // Where each button takes clicks; touching, so a button group can lay them out.
};

// The buttons' capsule at the right end of a title bar. `edgeToEdge` (a maximized window) stretches
// the hit areas up to the screen edge and the last one into the corner. Returns an empty layout when
// `buttonCount` is zero or less.
[[nodiscard]] TitleLayout layoutTitleBar(qreal windowWidth, qreal titleBarHeight, int buttonCount, bool edgeToEdge);

// Where a title `textWidth` wide goes: centred on the window, moved left rather than under the
// buttons, and cut to the space left when it is wider than that. `capsule` is empty without buttons.
[[nodiscard]] QRectF captionRect(qreal windowWidth, qreal titleBarHeight, qreal textWidth, const QRectF &capsule);

} // namespace aurora
