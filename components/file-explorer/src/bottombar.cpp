// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements bottombar.h.

#include "bottombar.h"

#include "crumbstrip.h"
#include "style.h"

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

namespace fe {

namespace {

constexpr qreal PATH_LEFT = 14.0;
/// Space kept free between the path and the status text at the right end.
constexpr qreal STATUS_GAP = 24.0;
constexpr qreal STATUS_RIGHT = 14.0;

} // namespace

PathBar::PathBar(QWidget *parent)
    : QWidget(parent)
    , m_crumbs(new CrumbStrip(this, true, std::nullopt))
{
    setFixedHeight(style::PATHBAR_HEIGHT);
    setMouseTracking(true);
    setAcceptDrops(true);
    setFont(style::uiFont(12));
}

CrumbStrip *PathBar::crumbs() const
{
    return m_crumbs;
}

void PathBar::setCrumbs(const QList<Crumb> &crumbs, const QString &lastIconName, bool lastIsFolder)
{
    m_crumbs->setCrumbs(crumbs, lastIconName, lastIsFolder);
}

void PathBar::setStatusText(const QString &text)
{
    if (text != m_status) {
        m_status = text;
        placeCrumbs();
        update();
    }
}

qreal PathBar::statusWidth() const
{
    return m_status.isEmpty() ? 0.0 : QFontMetricsF(font()).horizontalAdvance(m_status) + STATUS_GAP + STATUS_RIGHT;
}

void PathBar::placeCrumbs()
{
    m_crumbs->setArea(QRectF(PATH_LEFT, 0, std::max(0.0, width() - PATH_LEFT - statusWidth()), height()));
}

bool PathBar::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        const auto *help = static_cast<QHelpEvent *>(event);
        const QString tip = m_crumbs->toolTipAt(help->pos());
        if (tip.isEmpty()) {
            QToolTip::hideText();
            event->ignore();
        } else {
            QToolTip::showText(help->globalPos(), tip, this);
        }
        return true;
    }
    return QWidget::event(event);
}

void PathBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!m_status.isEmpty()) {
        painter.setFont(font());
        painter.setPen(style::TEXT_SECONDARY);
        painter.drawText(QRectF(rect()).adjusted(0, 0, -STATUS_RIGHT, 0), Qt::AlignRight | Qt::AlignVCenter, m_status);
    }
    m_crumbs->paint(painter);
}

void PathBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    placeCrumbs();
}

void PathBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_crumbs->mouseMove(event)) {
        QWidget::mouseMoveEvent(event);
    }
}

void PathBar::mousePressEvent(QMouseEvent *event)
{
    // Presses on the empty part of the bar fall through to the window, which moves it.
    if (!m_crumbs->mousePress(event)) {
        event->ignore();
    }
}

void PathBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_crumbs->mouseRelease(event)) {
        event->ignore();
    }
}

void PathBar::leaveEvent(QEvent *event)
{
    m_crumbs->leave();
    QWidget::leaveEvent(event);
}

void PathBar::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_crumbs->contextMenu(event)) {
        event->ignore();
    }
}

void PathBar::dragEnterEvent(QDragEnterEvent *event)
{
    if (!m_crumbs->dragEnter(event)) {
        event->ignore();
    }
}

void PathBar::dragMoveEvent(QDragMoveEvent *event)
{
    if (!m_crumbs->dragMove(event)) {
        event->ignore();
    }
}

void PathBar::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_crumbs->dragLeave();
    QWidget::dragLeaveEvent(event);
}

void PathBar::dropEvent(QDropEvent *event)
{
    if (!m_crumbs->drop(event)) {
        event->ignore();
    }
}

} // namespace fe
