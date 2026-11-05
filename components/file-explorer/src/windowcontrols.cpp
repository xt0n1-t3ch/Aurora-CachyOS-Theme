// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements windowcontrols.h. The capsule, the hover pills and the glyphs are painted by hand to match the
// toolbar's capsules.

#include "windowcontrols.h"

#include "style.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

enum Segment {
    MINIMIZE,
    MAXIMIZE,
    CLOSE,
    SEGMENT_COUNT,
};

// The gap between the capsule's end and its first and last button.
constexpr int PADDING = 3;
constexpr qreal GLYPH_PEN = 1.4;
constexpr qreal GLYPH_HALF = 4.8;
const QColor CLOSE_HOVER{255, 69, 58};
const QColor CLOSE_PRESSED{214, 52, 44};

/// Snaps a coordinate to the middle of a pixel, so a one-pixel line stays sharp.
qreal crisp(qreal value)
{
    return std::floor(value) + 0.5;
}

/// Draws the glyph of a segment centered on center; the maximize segment shows the restore glyph when maximized.
void drawGlyph(QPainter &painter, int segment, bool maximized, QPointF center, const QColor &color)
{
    const QPointF c(crisp(center.x()), crisp(center.y()));
    const qreal h = GLYPH_HALF;
    painter.setPen(QPen(color, GLYPH_PEN, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    switch (segment) {
    case MINIMIZE:
        painter.drawLine(QPointF(c.x() - h, c.y()), QPointF(c.x() + h, c.y()));
        break;
    case MAXIMIZE:
        if (!maximized) {
            painter.drawRoundedRect(QRectF(c.x() - h, c.y() - h, 2 * h, 2 * h), 2.2, 2.2);
            break;
        }
        {
            // Restore: a front square and the corner of the one behind it, as Windows draws it.
            const qreal side = 2 * h - 2.2;
            const QRectF front(c.x() - h, c.y() - h + 2.2, side, side);
            const QRectF back = front.translated(2.2, -2.2);
            painter.save();
            painter.setClipRegion(QRegion(0, 0, painter.device()->width(), painter.device()->height())
                                  - QRegion(front.adjusted(-1.2, -1.2, 1.2, 1.2).toAlignedRect()));
            painter.drawRoundedRect(back, 1.8, 1.8);
            painter.restore();
            painter.drawRoundedRect(front, 1.8, 1.8);
        }
        break;
    default:
        painter.drawLine(QPointF(c.x() - h, c.y() - h), QPointF(c.x() + h, c.y() + h));
        painter.drawLine(QPointF(c.x() - h, c.y() + h), QPointF(c.x() + h, c.y() - h));
        break;
    }
}

/// The tooltip of a segment; empty for no segment (-1).
QString toolTipFor(int segment, bool maximized)
{
    switch (segment) {
    case MINIMIZE:
        return QStringLiteral("Minimize");
    case MAXIMIZE:
        return maximized ? QStringLiteral("Restore") : QStringLiteral("Maximize");
    case CLOSE:
        return QStringLiteral("Close");
    default:
        return {};
    }
}

} // namespace

WindowControls::WindowControls(QWidget *parent)
    : QWidget(parent)
    , m_fade(new fe::motion::HoverFade(fe::style::HOVER_FADE_MS, this))
{
    connect(m_fade, &fe::motion::HoverFade::changed, this, qOverload<>(&QWidget::update));
    setMouseTracking(true);
    setFixedSize(sizeHint());
}

QSize WindowControls::sizeHint() const
{
    return {2 * PADDING + SEGMENT_COUNT * fe::style::BUTTON_WIDTH, fe::style::CAPSULE_HEIGHT};
}

void WindowControls::setMaximized(bool maximized)
{
    if (maximized != m_maximized) {
        m_maximized = maximized;
        setToolTip(toolTipFor(m_hover, m_maximized));
        update();
    }
}

QRectF WindowControls::segmentRect(int index) const
{
    return {static_cast<qreal>(PADDING + index * fe::style::BUTTON_WIDTH), 0.0, static_cast<qreal>(fe::style::BUTTON_WIDTH),
            static_cast<qreal>(height())};
}

int WindowControls::segmentAt(QPointF pos) const
{
    for (int index = 0; index < SEGMENT_COUNT; ++index) {
        if (segmentRect(index).contains(pos)) {
            return index;
        }
    }
    return -1;
}

void WindowControls::setHover(int index)
{
    if (index != m_hover) {
        m_fade->reset();
        m_fade->setHovered(index >= 0);
        m_hover = index;
        setToolTip(toolTipFor(index, m_maximized));
        // The hand over a button. The arrow is set, not inherited, so a resize cursor of the window's edge never shows here.
        setCursor(index >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void WindowControls::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds(rect());
    fe::style::paintGlass(painter, bounds, bounds.height() / 2.0, fe::style::CAPSULE_FILL);

    const bool active = isActiveWindow();
    for (int index = 0; index < SEGMENT_COUNT; ++index) {
        const QRectF segment = segmentRect(index);
        const bool hovered = m_hover == index;
        const bool pressed = hovered && m_pressed == index;
        const bool close = index == CLOSE;
        if (hovered) {
            // The same inset pill as a hovered toolbar button (GlassButton), red for close.
            const QRectF pill = segment.adjusted(2, 3, -2, -3);
            QColor fill = pressed ? fe::style::CAPSULE_PRESSED : fe::style::CAPSULE_HOVER;
            if (close) {
                fill = pressed ? CLOSE_PRESSED : CLOSE_HOVER;
            }
            if (!pressed) {
                fill.setAlphaF(fill.alphaF() * m_fade->progress());
            }
            QPainterPath path;
            path.addRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);
            painter.fillPath(path, fill);
        }
        QColor glyph = active || hovered ? fe::style::TEXT : fe::style::TEXT_TERTIARY;
        if (close && hovered) {
            glyph = Qt::white;
        }
        drawGlyph(painter, index, m_maximized, segment.center(), glyph);
    }
}

void WindowControls::mouseMoveEvent(QMouseEvent *event)
{
    setHover(segmentAt(event->position()));
    QWidget::mouseMoveEvent(event);
}

void WindowControls::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_pressed = segmentAt(event->position());
    update();
    event->accept();
}

void WindowControls::mouseReleaseEvent(QMouseEvent *event)
{
    const int pressed = m_pressed;
    m_pressed = -1;
    update();
    // A button acts only when the press and the release land on it, like every desktop button.
    if (event->button() != Qt::LeftButton || pressed < 0 || segmentAt(event->position()) != pressed) {
        return;
    }
    switch (pressed) {
    case MINIMIZE:
        Q_EMIT minimizeClicked();
        break;
    case MAXIMIZE:
        Q_EMIT maximizeClicked();
        break;
    default:
        Q_EMIT closeClicked();
        break;
    }
}

void WindowControls::leaveEvent(QEvent *event)
{
    m_pressed = -1;
    setHover(-1);
    QWidget::leaveEvent(event);
}

void WindowControls::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange) {
        update();
    }
}
