// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements tabbar.h.

#include "tabbar.h"

#include "style.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace fe {

namespace {

constexpr qreal TRACK_HEIGHT = 26.0;
constexpr qreal PLUS_SIZE = 26.0;
constexpr qreal PLUS_GAP = 8.0;
constexpr qreal CLOSE_SIZE = 16.0;
constexpr qreal TAB_INSET = 2.0;
const QColor CURRENT_TAB_FILL{255, 255, 255, 40};

// A close cross in the middle of box.
void drawCross(QPainter &painter, const QRectF &box, const QColor &color)
{
    const QPointF c = box.center();
    const qreal s = box.width() * 0.2;
    painter.setPen(QPen(color, 1.3, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(c.x() - s, c.y() - s), QPointF(c.x() + s, c.y() + s));
    painter.drawLine(QPointF(c.x() - s, c.y() + s), QPointF(c.x() + s, c.y() - s));
}

} // namespace

PillTabBar::PillTabBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(style::TABBAR_HEIGHT);
    setMouseTracking(true);
    hide();
}

void PillTabBar::insertTab(int index, const QString &title)
{
    index = qBound(0, index, count());
    m_titles.insert(index, title);
    if (m_current >= index) {
        ++m_current;
    }
    if (m_current < 0) {
        m_current = 0;
    }
    updateVisibility();
    update();
}

void PillTabBar::removeTab(int index)
{
    if (index < 0 || index >= count()) {
        return;
    }
    m_titles.removeAt(index);
    if (m_current > index || m_current >= count()) {
        --m_current;
    }
    m_hover = {};
    updateVisibility();
    update();
}

void PillTabBar::setTabTitle(int index, const QString &title)
{
    if (index >= 0 && index < count()) {
        m_titles[index] = title;
        update();
    }
}

void PillTabBar::setCurrentIndex(int index)
{
    if (index >= 0 && index < count() && index != m_current) {
        m_current = index;
        update();
    }
}

int PillTabBar::currentIndex() const
{
    return m_current;
}

int PillTabBar::count() const
{
    return static_cast<int>(m_titles.size());
}

void PillTabBar::updateVisibility()
{
    setVisible(count() > 1);
}

QRectF PillTabBar::trackRect() const
{
    const qreal top = (height() - TRACK_HEIGHT) / 2.0;
    return {0.0, top, width() - PLUS_SIZE - PLUS_GAP, TRACK_HEIGHT};
}

QRectF PillTabBar::tabRect(int index) const
{
    const QRectF track = trackRect();
    const qreal width = track.width() / qMax(1, count());
    return {track.left() + index * width, track.top(), width, track.height()};
}

QRectF PillTabBar::closeRect(int index) const
{
    const QRectF tab = tabRect(index);
    return {tab.left() + 8.0, tab.center().y() - CLOSE_SIZE / 2.0, CLOSE_SIZE, CLOSE_SIZE};
}

QRectF PillTabBar::plusRect() const
{
    return {width() - PLUS_SIZE, (height() - PLUS_SIZE) / 2.0, PLUS_SIZE, PLUS_SIZE};
}

PillTabBar::Hit PillTabBar::hitAt(QPointF pos) const
{
    if (plusRect().contains(pos)) {
        return {Part::Plus, -1};
    }
    for (int index = 0; index < count(); ++index) {
        if (tabRect(index).contains(pos)) {
            return {closeRect(index).contains(pos) ? Part::Close : Part::Tab, index};
        }
    }
    return {};
}

void PillTabBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF track = trackRect();
    style::paintGlass(painter, track, track.height() / 2.0, style::CAPSULE_FILL);

    for (int index = 0; index < count(); ++index) {
        const QRectF tab = tabRect(index);
        const bool current = index == m_current;
        const bool hovered = m_hover.index == index && m_hover.part != Part::None;
        if (current) {
            const QRectF raised = tab.adjusted(TAB_INSET, TAB_INSET, -TAB_INSET, -TAB_INSET);
            style::paintGlass(painter, raised, raised.height() / 2.0, CURRENT_TAB_FILL);
        } else if (hovered) {
            QPainterPath path;
            const QRectF inner = tab.adjusted(TAB_INSET, TAB_INSET, -TAB_INSET, -TAB_INSET);
            path.addRoundedRect(inner, inner.height() / 2.0, inner.height() / 2.0);
            painter.fillPath(path, style::HOVER);
        }
        // A hairline between two tabs that are both flat.
        if (index > 0 && index != m_current && index - 1 != m_current) {
            painter.setPen(QPen(style::HAIRLINE, 1.0));
            painter.drawLine(QPointF(tab.left() + 0.5, tab.top() + 7), QPointF(tab.left() + 0.5, tab.bottom() - 7));
        }

        painter.setFont(style::uiFont(12, current ? QFont::DemiBold : QFont::Normal));
        painter.setPen(current ? style::TEXT : style::TEXT_SECONDARY);
        const QRectF textRect = tab.adjusted(CLOSE_SIZE + 14.0, 0, -(CLOSE_SIZE + 14.0), 0);
        const QString title = painter.fontMetrics().elidedText(m_titles.at(index), Qt::ElideMiddle, qRound(textRect.width()));
        painter.drawText(textRect, Qt::AlignCenter, title);

        if (hovered) {
            const QRectF close = closeRect(index);
            if (m_hover.part == Part::Close) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(style::CAPSULE_HOVER);
                painter.drawEllipse(close);
            }
            drawCross(painter, close, style::TEXT_SECONDARY);
        }
    }

    const QRectF plus = plusRect();
    style::paintGlass(painter, plus, plus.height() / 2.0, m_hover.part == Part::Plus ? style::CAPSULE_HOVER : style::CAPSULE_FILL);
    const int iconSize = 14;
    const QPixmap icon = style::tintedIcon(QStringLiteral("list-add-symbolic"), iconSize, style::TEXT, devicePixelRatioF());
    painter.drawPixmap(QPointF(plus.center().x() - iconSize / 2.0, plus.center().y() - iconSize / 2.0), icon);
}

void PillTabBar::mouseMoveEvent(QMouseEvent *event)
{
    const Hit hit = hitAt(event->position());
    if (hit.part != m_hover.part || hit.index != m_hover.index) {
        m_hover = hit;
        setCursor(hit.part == Part::None ? Qt::ArrowCursor : Qt::PointingHandCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void PillTabBar::mousePressEvent(QMouseEvent *event)
{
    const Hit hit = hitAt(event->position());
    if (event->button() == Qt::MiddleButton && hit.index >= 0) {
        Q_EMIT tabCloseRequested(hit.index);
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_pressed = hit;
    // Like Finder, a tab switches on press; close and "+" act on release over the same spot.
    if (hit.part == Part::Tab) {
        Q_EMIT tabClicked(hit.index);
    }
    event->accept();
}

void PillTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    const Hit pressed = m_pressed;
    m_pressed = {};
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const Hit hit = hitAt(event->position());
    if (hit.part != pressed.part || hit.index != pressed.index) {
        return;
    }
    if (hit.part == Part::Close) {
        Q_EMIT tabCloseRequested(hit.index);
    } else if (hit.part == Part::Plus) {
        Q_EMIT newTabClicked();
    }
}

void PillTabBar::leaveEvent(QEvent *event)
{
    m_hover = {};
    setCursor(Qt::ArrowCursor);
    update();
    QWidget::leaveEvent(event);
}

} // namespace fe
