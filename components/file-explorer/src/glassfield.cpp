// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements glassfield.h.

#include "glassfield.h"

#include "style.h"

#include <QPainter>

namespace fe {

namespace {

constexpr int ICON_SIZE = 13;
constexpr int ICON_LEFT = 9;

} // namespace

GlassLineEdit::GlassLineEdit(const QString &iconName, int height, QWidget *parent)
    : QLineEdit(parent)
    , m_iconName(iconName)
{
    setFrame(false);
    setFont(style::uiFont(12));
    setFixedHeight(height);
    setTextMargins(iconName.isEmpty() ? 10 : ICON_LEFT + ICON_SIZE + 4, 0, 10, 0);
    QPalette colors = palette();
    colors.setColor(QPalette::Base, Qt::transparent);
    colors.setColor(QPalette::Text, style::TEXT);
    colors.setColor(QPalette::PlaceholderText, style::TEXT_SECONDARY);
    setPalette(colors);
}

void GlassLineEdit::paintEvent(QPaintEvent *event)
{
    {
        QPainter painter(this);
        const QRectF bounds(rect());
        style::paintGlass(painter, bounds, bounds.height() / 2.0, hasFocus() ? style::CAPSULE_HOVER : style::CAPSULE_FILL);
        if (hasFocus()) {
            style::paintFocusRing(painter, bounds.adjusted(1, 1, -1, -1));
        }
        if (!m_iconName.isEmpty()) {
            const QPixmap symbol = style::tintedIcon(m_iconName, ICON_SIZE, style::TEXT_SECONDARY, devicePixelRatioF());
            painter.drawPixmap(QPointF(ICON_LEFT, (height() - ICON_SIZE) / 2.0), symbol);
        }
    }
    QLineEdit::paintEvent(event);
}

} // namespace fe
