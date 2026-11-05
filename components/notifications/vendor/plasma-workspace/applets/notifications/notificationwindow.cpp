/*
  SPDX-FileCopyrightText: 2024 David Edmundson <davidedmundson@kde.org>
  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "notificationwindow.h"

#include <KWindowSystem>
#include <QRegion>
#include <QResizeEvent>
#include <KX11Extras>
#include <PlasmaQuick/PlasmaShellWaylandIntegration>

// Aurora: the banner draws its own glass (NotificationPopup.qml), so the window takes no theme frame: an image
// path the theme does not have leaves PlasmaWindow without background, margins or shadow.
NotificationWindow::NotificationWindow()
    : PlasmaQuick::PlasmaWindow(QStringLiteral("aurora/no-frame"))
{
    PlasmaShellWaylandIntegration::get(this)->setRole(QtWayland::org_kde_plasma_surface::role_notification);
    PlasmaShellWaylandIntegration::get(this)->setTakesFocus(false);

    if (KWindowSystem::isPlatformX11()) {
        KX11Extras::setOnAllDesktops(winId(), true);
        KX11Extras::setType(winId(), NET::Notification);
    }
}

NotificationWindow::~NotificationWindow() = default;

bool NotificationWindow::takeFocus() const
{
    return m_takeFocus;
}

void NotificationWindow::setTakeFocus(bool takeFocus)
{
    if (m_takeFocus == takeFocus) {
        return;
    }
    PlasmaShellWaylandIntegration::get(this)->setTakesFocus(takeFocus);
    QWindow::setFlag(Qt::WindowDoesNotAcceptFocus, !takeFocus);

    m_takeFocus = takeFocus;
    Q_EMIT takeFocusChanged();
}

bool NotificationWindow::isCritical() const
{
    return m_critical;
}

void NotificationWindow::setIsCritical(bool critical)
{
    if (m_critical == critical) {
        return;
    }

    m_critical = critical;

    auto role = critical ? QtWayland::org_kde_plasma_surface::role_criticalnotification : QtWayland::org_kde_plasma_surface::role_notification;
    PlasmaShellWaylandIntegration::get(this)->setRole(role);

    if (KWindowSystem::isPlatformX11()) {
        KX11Extras::setType(winId(), critical ? NET::CriticalNotification : NET::Notification);
    }

    Q_EMIT isCriticalChanged();
}

bool NotificationWindow::event(QEvent *e)
{
    // moveEvents only occur when we have a platform window, so we need to explicitly sync on creation
    if (e->type() == QEvent::PlatformSurface) {
        auto ps = static_cast<QPlatformSurfaceEvent *>(e);
        if (ps->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
            PlasmaShellWaylandIntegration::get(this)->setPosition(QPoint(x(), y()));
        }
    }
    return PlasmaWindow::event(e);
}

void NotificationWindow::moveEvent(QMoveEvent *me)
{
    PlasmaShellWaylandIntegration::get(this)->setPosition(me->pos());
    PlasmaWindow::moveEvent(me);
    update();
}

#include "moc_notificationwindow.cpp"

QRectF NotificationWindow::inputRect() const
{
    return m_inputRect;
}

void NotificationWindow::setInputRect(const QRectF &rect)
{
    if (m_inputRect == rect) {
        return;
    }
    m_inputRect = rect;
    setMask(QRegion(rect.toAlignedRect()));
    Q_EMIT inputRectChanged();
}

void NotificationWindow::resizeEvent(QResizeEvent *event)
{
    PlasmaQuick::PlasmaWindow::resizeEvent(event);
    // PlasmaWindow resets the mask from its (empty) frame when the frame changes size.
    if (!m_inputRect.isEmpty()) {
        setMask(QRegion(m_inputRect.toAlignedRect()));
    }
}
