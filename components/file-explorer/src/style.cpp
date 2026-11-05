// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements style.h.

#include "style.h"

#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

namespace fe::style {

QFont uiFont(qreal pixelSize, QFont::Weight weight)
{
    QFont font;
    const QString family = pixelSize >= 20 ? QStringLiteral("SF Pro Display") : QStringLiteral("SF Pro Text");
    font.setFamilies({family, QStringLiteral("SF Pro Text"), QStringLiteral("Inter"), QStringLiteral("Noto Sans")});
    font.setPixelSize(qRound(pixelSize));
    font.setWeight(weight);
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

QPixmap tintedIcon(const QString &name, int size, const QColor &color, qreal devicePixelRatio)
{
    const QString key = QStringLiteral("fe-tint-%1-%2-%3-%4").arg(name).arg(size).arg(color.rgba()).arg(devicePixelRatio);
    QPixmap cached;
    if (QPixmapCache::find(key, &cached)) {
        return cached;
    }

    // ":/icons/..." names are the app's own SF-style symbols; the rest come from the icon theme.
    const QIcon icon = name.startsWith(u':') ? QIcon(name) : QIcon::fromTheme(name, QIcon::fromTheme(QStringLiteral("folder-symbolic")));
    const QPixmap source = icon.pixmap(QSize(size, size), devicePixelRatio);
    QPixmap tinted(source.size());
    tinted.setDevicePixelRatio(source.devicePixelRatio());
    tinted.fill(Qt::transparent);
    {
        QPainter painter(&tinted);
        painter.drawPixmap(0, 0, source);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(QRect(QPoint(0, 0), source.size()), color);
    }
    QPixmapCache::insert(key, tinted);
    return tinted;
}

void paintGlass(QPainter &painter, const QRectF &rect, qreal radius, const QColor &fill)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath body;
    body.addRoundedRect(rect, radius, radius);
    painter.fillPath(body, fill);

    QLinearGradient rim(rect.topLeft(), rect.bottomLeft());
    rim.setColorAt(0.0, GLASS_RIM_TOP);
    rim.setColorAt(0.35, GLASS_RIM);
    rim.setColorAt(1.0, QColor(255, 255, 255, 16));
    painter.setPen(QPen(QBrush(rim), 1.0));
    painter.setBrush(Qt::NoBrush);
    const QRectF edge = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    painter.drawRoundedRect(edge, qMax(0.0, radius - 0.5), qMax(0.0, radius - 0.5));

    painter.restore();
}

QPalette darkPalette()
{
    // macOS dark system colors (windowBackgroundColor, controlBackgroundColor, labelColor, ...).
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(30, 30, 32));
    palette.setColor(QPalette::WindowText, TEXT);
    palette.setColor(QPalette::Base, QColor(22, 22, 24));
    palette.setColor(QPalette::AlternateBase, QColor(255, 255, 255, 10));
    palette.setColor(QPalette::Text, TEXT);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Button, QColor(60, 60, 64));
    palette.setColor(QPalette::ButtonText, TEXT);
    palette.setColor(QPalette::Highlight, SELECTION);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, QColor(44, 44, 48));
    palette.setColor(QPalette::ToolTipText, TEXT);
    palette.setColor(QPalette::PlaceholderText, TEXT_SECONDARY);
    palette.setColor(QPalette::Link, ACCENT);
    palette.setColor(QPalette::LinkVisited, QColor(191, 90, 242));
    palette.setColor(QPalette::Light, QColor(84, 84, 88));
    palette.setColor(QPalette::Midlight, QColor(66, 66, 70));
    palette.setColor(QPalette::Mid, QColor(48, 48, 52));
    palette.setColor(QPalette::Dark, QColor(20, 20, 22));
    palette.setColor(QPalette::Shadow, Qt::black);
    palette.setColor(QPalette::Accent, ACCENT);
    palette.setColor(QPalette::Inactive, QPalette::Highlight, QColor(70, 70, 74));
    for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        palette.setColor(QPalette::Disabled, role, TEXT_TERTIARY);
    }
    return palette;
}

} // namespace fe::style
