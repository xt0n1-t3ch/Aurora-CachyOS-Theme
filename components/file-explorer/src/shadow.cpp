// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements shadow.h. Builds one shadow image, cuts it into the eight tiles KWin takes, and hands them over.

#include "shadow.h"

#include <KWindowShadow>

#include <QImage>
#include <QPainter>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <vector>

namespace fe {

namespace {

const auto SHADOW_OBJECT_NAME = QStringLiteral("fe-window-shadow");

/// Box-blurs the alpha channel of image along rows, then columns; three passes approximate a Gaussian.
void boxBlurAlpha(QImage &image, int radius)
{
    const int width = image.width();
    const int height = image.height();
    std::vector<int> line(static_cast<size_t>(std::max(width, height)));
    const auto blurLine = [&](auto read, auto write, int length) {
        // A running sum over a window of 2 * radius + 1 pixels; pixels past either end repeat the edge pixel.
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

/// A square image of the whole shadow around a minimal body, cut out where the body is, so the
/// shadow never darkens the translucent window itself.
QImage shadowImage(const ShadowSpec &spec)
{
    // The odd side leaves one middle pixel row and column, which the edge tiles repeat along each side.
    const int side = 2 * (spec.extent + spec.radius) + 1;
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const QRectF body(spec.extent, spec.extent, side - 2 * spec.extent, side - 2 * spec.extent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, spec.alpha));
        painter.drawRoundedRect(body.translated(0, spec.offsetY), spec.radius, spec.radius);
    }
    boxBlurAlpha(image, spec.blur);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    painter.drawRoundedRect(body, spec.radius, spec.radius);
    return image;
}

/// The part of image inside rect as a tile for KWin.
KWindowShadowTile::Ptr tileFrom(const QImage &image, const QRect &rect)
{
    auto tile = KWindowShadowTile::Ptr::create();
    tile->setImage(image.copy(rect));
    return tile;
}

} // namespace

void attachShadow(QWidget *window, const ShadowSpec &spec)
{
    if (!window || !window->windowHandle()) {
        return;
    }
    detachShadow(window);

    auto *shadow = new KWindowShadow(window);
    shadow->setObjectName(SHADOW_OBJECT_NAME);
    const QImage image = shadowImage(spec);
    const int side = image.width();
    const int corner = spec.extent + spec.radius;
    const int extent = spec.extent;
    shadow->setTopLeftTile(tileFrom(image, QRect(0, 0, corner, corner)));
    shadow->setTopTile(tileFrom(image, QRect(corner, 0, 1, extent)));
    shadow->setTopRightTile(tileFrom(image, QRect(side - corner, 0, corner, corner)));
    shadow->setRightTile(tileFrom(image, QRect(side - extent, corner, extent, 1)));
    shadow->setBottomRightTile(tileFrom(image, QRect(side - corner, side - corner, corner, corner)));
    shadow->setBottomTile(tileFrom(image, QRect(corner, side - extent, 1, extent)));
    shadow->setBottomLeftTile(tileFrom(image, QRect(0, side - corner, corner, corner)));
    shadow->setLeftTile(tileFrom(image, QRect(0, corner, extent, 1)));
    shadow->setPadding(QMargins(extent, extent, extent, extent));
    shadow->setWindow(window->windowHandle());
    shadow->create();
}

void detachShadow(QWidget *window)
{
    if (!window) {
        return;
    }
    if (auto *shadow = window->findChild<KWindowShadow *>(SHADOW_OBJECT_NAME, Qt::FindDirectChildrenOnly)) {
        shadow->destroy();
        delete shadow;
    }
}

} // namespace fe
