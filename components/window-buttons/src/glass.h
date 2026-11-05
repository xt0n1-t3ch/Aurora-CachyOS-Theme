// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The painted pieces of the Aurora decoration: glass capsule, button strokes, title bar shape and shadow image.

#pragma once

#include <QColor>
#include <QImage>
#include <QPainterPath>
#include <QRectF>

class QPainter;

namespace aurora {

// Radius in pixels of the window's rounded corners.
inline constexpr qreal CORNER_RADIUS = 14;

// Whether a background is dark, so glass and glyphs pick light ink on it.
[[nodiscard]] bool isDark(const QColor &background);

// The title bar shape: rounded at the top only, or at all four corners for a shaded window. A `radius` of
// zero or less gives a plain rectangle.
[[nodiscard]] QPainterPath titleBarPath(const QRectF &bar, qreal radius, bool allCorners);

// A Liquid Glass capsule: a translucent fill and a rim that catches light at the top, the same as
// the File Explorer's toolbar capsules. `onDark` picks light ink; `active` makes the fill stronger.
// Draws into `rect` and leaves the painter's state as it was.
void paintCapsule(QPainter &painter, const QRectF &rect, bool onDark, bool active);

// The minimize, maximize (or restore) and close strokes of the File Explorer's window buttons.
enum class Glyph {
    Minimize,
    Maximize,
    Restore,
    Close,
};
// Strokes `glyph` in `color`, centred on the pixel that contains `center` so one-pixel lines stay sharp.
// Leaves the painter's state as it was.
void paintGlyph(QPainter &painter, Glyph glyph, QPointF center, const QColor &color);

// A soft shadow around a body with corners of `radius`: blurred by `blur` pixels, pushed down by `offsetY`
// and cut out where the body is. `alpha` (0 to 255) is the shadow's opacity before the blur. The body sits
// `extent` pixels inside every edge of the returned square image, which is 2 * (extent + radius) + 1 pixels wide.
[[nodiscard]] QImage shadowImage(int radius, int extent, int offsetY, int blur, int alpha);

} // namespace aurora
