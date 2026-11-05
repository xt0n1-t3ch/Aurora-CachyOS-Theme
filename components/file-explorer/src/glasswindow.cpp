// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements glasswindow.h.

#include "glasswindow.h"

#include "core/edges.h"
#include "shadow.h"
#include "style.h"

#include <KWindowEffects>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWindow>

#include <algorithm>

namespace {

// A large, soft shadow pushed down, like the shadow of an active macOS window.
constexpr fe::ShadowSpec WINDOW_SHADOW{fe::style::WINDOW_RADIUS, 64, 14, 15, 150};

} // namespace

GlassWindow::GlassWindow(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    // Every view inside the window lets the glass show through; separate windows (dialogs, menus)
    // do not inherit this and keep the opaque application palette.
    QPalette glass = palette();
    glass.setColor(QPalette::Base, Qt::transparent);
    glass.setColor(QPalette::Window, Qt::transparent);
    setPalette(glass);
}

void GlassWindow::addDragSurface(QWidget *surface)
{
    m_dragSurfaces.append(surface);
    surface->installEventFilter(this);
}

void GlassWindow::toggleMaximized()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

qreal GlassWindow::cornerRadius() const
{
    return (isMaximized() || isFullScreen()) ? 0.0 : fe::style::WINDOW_RADIUS;
}

QPainterPath GlassWindow::glassCutout() const
{
    return {};
}

void GlassWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal radius = cornerRadius();
    QPainterPath body;
    body.addRoundedRect(QRectF(rect()), radius, radius);
    const QPainterPath cutout = glassCutout();
    const QPainterPath tinted = cutout.isEmpty() ? body : body.subtracted(cutout);
    painter.fillPath(tinted, isActiveWindow() ? fe::style::WINDOW_TINT : fe::style::WINDOW_TINT_INACTIVE);
    painter.setPen(QPen(fe::style::WINDOW_RIM, 1.0));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), qMax(0.0, radius - 0.5), qMax(0.0, radius - 0.5));
}

void GlassWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateBlurRegion();
}

void GlassWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateBlurRegion();
    updateShadow();
}

void GlassWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateBlurRegion();
        updateShadow();
        update();
    } else if (event->type() == QEvent::ActivationChange) {
        update();
    }
}

void GlassWindow::mouseMoveEvent(QMouseEvent *event)
{
    const Qt::Edges edges = isMaximized() ? Qt::Edges() : fe::resizeEdgesAt(event->position().toPoint(), size(), fe::style::RESIZE_BORDER);
    if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
        setCursor(Qt::SizeHorCursor);
    } else if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
        setCursor(Qt::SizeVerCursor);
    } else {
        unsetCursor();
    }
    QWidget::mouseMoveEvent(event);
}

void GlassWindow::mousePressEvent(QMouseEvent *event)
{
    // A press no child handled lands here: in the edge band it resizes the window, anywhere else it moves it.
    if (event->button() == Qt::LeftButton && windowHandle()) {
        const Qt::Edges edges = isMaximized() ? Qt::Edges() : fe::resizeEdgesAt(event->position().toPoint(), size(), fe::style::RESIZE_BORDER);
        if (edges) {
            windowHandle()->startSystemResize(edges);
        } else {
            windowHandle()->startSystemMove();
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void GlassWindow::leaveEvent(QEvent *event)
{
    unsetCursor();
    QWidget::leaveEvent(event);
}

bool GlassWindow::eventFilter(QObject *watched, QEvent *event)
{
    const bool isSurface = std::any_of(m_dragSurfaces.cbegin(), m_dragSurfaces.cend(),
                                       [watched](const QPointer<QWidget> &surface) { return surface == watched; });
    if (isSurface && windowHandle()
        && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            if (event->type() == QEvent::MouseButtonDblClick) {
                toggleMaximized();
            } else {
                windowHandle()->startSystemMove();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void GlassWindow::updateBlurRegion()
{
    if (!windowHandle()) {
        return;
    }
    QPainterPath body;
    body.addRoundedRect(QRectF(rect()), cornerRadius(), cornerRadius());
    KWindowEffects::enableBlurBehind(windowHandle(), true, QRegion(body.toFillPolygon().toPolygon()));
}

void GlassWindow::updateShadow()
{
    if (isMaximized() || isFullScreen()) {
        fe::detachShadow(this);
    } else {
        fe::attachShadow(this, WINDOW_SHADOW);
    }
}
