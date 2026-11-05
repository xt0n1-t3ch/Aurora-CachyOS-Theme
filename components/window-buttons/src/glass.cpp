// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Paints the glass capsule, the button strokes and the shadow image declared in glass.h.

#include "glass.h"

#include <QLinearGradient>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <vector>

namespace aurora {

namespace {

constexpr qreal GLYPH_PEN = 1.4;  // Stroke width in pixels.
constexpr qreal GLYPH_HALF = 4.8; // Half the side of a glyph in pixels.

// Snaps a coordinate to the middle of a pixel, so a one-pixel line stays sharp.
qreal crisp(qreal value)
{
    return std::floor(value) + 0.5;
}

// Box-blurs the alpha channel along rows, then columns; three passes approximate a Gaussian. The color is reset
// to black, which is all a shadow needs. Pixels past an edge repeat the edge pixel.
void boxBlurAlpha(QImage &image, int radius)
{
    const int width = image.width();
    const int height = image.height();
    std::vector<int> line(static_cast<size_t>(std::max(width, height)));
    const auto blurLine = [&](auto read, auto write, int length) {
        const int window = 2 * radius + 1;
        int sum = 0;
        for (int i = -radius; i <= radius; ++i) {
            sum += read(std::clamp(i, 0, length - 1));
        }
        for (int i = 0; i < length; ++i) {
            line[static_cast<size_t>(i)] = sum / window;
            sum += read(std::min(i + radius + 1, length - 1)) - read(std::max(i - radius, 0));
        }
        for (int i = 0; i < length; ++i) {
            write(i, line[static_cast<size_t>(i)]);
        }
    };
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < height; ++y) {
            auto *row = reinterpret_cast<QRgb *>(image.scanLine(y));
            blurLine([row](int x) { return qAlpha(row[x]); }, [row](int x, int a) { row[x] = qRgba(0, 0, 0, a); }, width);
        }
        for (int x = 0; x < width; ++x) {
            blurLine([&image, x](int y) { return qAlpha(reinterpret_cast<const QRgb *>(image.constScanLine(y))[x]); },
                     [&image, x](int y, int a) { reinterpret_cast<QRgb *>(image.scanLine(y))[x] = qRgba(0, 0, 0, a); },
                     height);
        }
    }
}

} // namespace

bool isDark(const QColor &background)
{
    return background.lightnessF() < 0.5;
}

QPainterPath titleBarPath(const QRectF &bar, qreal radius, bool allCorners)
{
    QPainterPath path;
    if (radius <= 0) {
        path.addRect(bar);
        return path;
    }
    if (allCorners) {
        path.addRoundedRect(bar, radius, radius);
        return path;
    }
    path.moveTo(bar.left(), bar.bottom());
    path.lineTo(bar.left(), bar.top() + radius);
    path.arcTo(QRectF(bar.left(), bar.top(), 2 * radius, 2 * radius), 180, -90);
    path.lineTo(bar.right() - radius, bar.top());
    path.arcTo(QRectF(bar.right() - 2 * radius, bar.top(), 2 * radius, 2 * radius), 90, -90);
    path.lineTo(bar.right(), bar.bottom());
    path.closeSubpath();
    return path;
}

void paintCapsule(QPainter &painter, const QRectF &rect, bool onDark, bool active)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal radius = rect.height() / 2;
    const QColor ink = onDark ? QColor(255, 255, 255) : QColor(0, 0, 0);
    QColor fill = ink;
    fill.setAlpha(onDark ? (active ? 22 : 12) : (active ? 16 : 9));
    QPainterPath body;
    body.addRoundedRect(rect, radius, radius);
    painter.fillPath(body, fill);

    QLinearGradient rim(rect.topLeft(), rect.bottomLeft());
    QColor top = QColor(255, 255, 255, onDark ? (active ? 58 : 30) : 150);
    QColor bottom = onDark ? QColor(255, 255, 255, 16) : QColor(0, 0, 0, 22);
    rim.setColorAt(0.0, top);
    rim.setColorAt(0.35, onDark ? QColor(255, 255, 255, active ? 26 : 14) : QColor(0, 0, 0, 10));
    rim.setColorAt(1.0, bottom);
    painter.setPen(QPen(QBrush(rim), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius - 0.5, radius - 0.5);
    painter.restore();
}

void paintGlyph(QPainter &painter, Glyph glyph, QPointF center, const QColor &color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF c(crisp(center.x()), crisp(center.y()));
    const qreal h = GLYPH_HALF;
    painter.setPen(QPen(color, GLYPH_PEN, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    switch (glyph) {
    case Glyph::Minimize:
        painter.drawLine(QPointF(c.x() - h, c.y()), QPointF(c.x() + h, c.y()));
        break;
    case Glyph::Maximize:
        painter.drawRoundedRect(QRectF(c.x() - h, c.y() - h, 2 * h, 2 * h), 2.2, 2.2);
        break;
    case Glyph::Restore: {
        // A front square and the corner of the one behind it, as Windows draws restore.
        const qreal side = 2 * h - 2.2;
        const QRectF front(c.x() - h, c.y() - h + 2.2, side, side);
        QPainterPath behind;
        behind.moveTo(front.left() + 2.2, front.top() - 0.2);
        behind.lineTo(front.left() + 2.2, front.top() - 2.2 + 1.8);
        behind.arcTo(QRectF(front.left() + 2.2, front.top() - 2.2, 3.6, 3.6), 180, -90);
        behind.lineTo(front.right() + 2.2 - 1.8, front.top() - 2.2);
        behind.arcTo(QRectF(front.right() + 2.2 - 3.6, front.top() - 2.2, 3.6, 3.6), 90, -90);
        behind.lineTo(front.right() + 2.2, front.bottom() - 2.2);
        behind.lineTo(front.right() + 0.2, front.bottom() - 2.2);
        painter.drawPath(behind);
        painter.drawRoundedRect(front, 1.8, 1.8);
        break;
    }
    case Glyph::Close:
        painter.drawLine(QPointF(c.x() - h, c.y() - h), QPointF(c.x() + h, c.y() + h));
        painter.drawLine(QPointF(c.x() - h, c.y() + h), QPointF(c.x() + h, c.y() - h));
        break;
    }
    painter.restore();
}

QImage shadowImage(int radius, int extent, int offsetY, int blur, int alpha)
{
    const int side = 2 * (extent + radius) + 1;
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const QRectF body(extent, extent, side - 2 * extent, side - 2 * extent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, alpha));
        painter.drawRoundedRect(body.translated(0, offsetY), radius, radius);
    }
    boxBlurAlpha(image, blur);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    // One pixel inside the body, so the shadow runs under the window edge and never leaves a gap.
    painter.drawRoundedRect(body.adjusted(1, 1, -1, -1), radius - 1, radius - 1);
    return image;
}

} // namespace aurora
