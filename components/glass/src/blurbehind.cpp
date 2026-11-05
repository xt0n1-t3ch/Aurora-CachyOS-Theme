// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Computes the blur region of a BlurBehind from its sources and keeps KWin's blur effects in step with it.
#include "blurbehind.h"

#include <KWindowEffects>

#include <QEvent>
#include <QQuickWindow>
#include <QTimer>

#include <cmath>

BlurBehind::BlurBehind(QQuickItem *parent)
    : QQuickItem(parent)
{
}

BlurBehind::~BlurBehind()
{
    if (m_window) {
        KWindowEffects::enableBlurBehind(m_window, false);
        KWindowEffects::enableBackgroundContrast(m_window, false);
    }
}

QQmlListProperty<QQuickItem> BlurBehind::sources()
{
    return QQmlListProperty<QQuickItem>(this, nullptr, &appendSource, &sourceCount, &sourceAt, &clearSources);
}

void BlurBehind::appendSource(QQmlListProperty<QQuickItem> *list, QQuickItem *item)
{
    auto *self = static_cast<BlurBehind *>(list->object);
    self->m_sources.append(item);
    self->m_dirty = true;
    Q_EMIT self->sourcesChanged();
    if (self->window()) {
        self->window()->update();
    }
}

qsizetype BlurBehind::sourceCount(QQmlListProperty<QQuickItem> *list)
{
    return static_cast<BlurBehind *>(list->object)->m_sources.size();
}

QQuickItem *BlurBehind::sourceAt(QQmlListProperty<QQuickItem> *list, qsizetype index)
{
    return static_cast<BlurBehind *>(list->object)->m_sources.value(index);
}

void BlurBehind::clearSources(QQmlListProperty<QQuickItem> *list)
{
    auto *self = static_cast<BlurBehind *>(list->object);
    self->m_sources.clear();
    self->m_dirty = true;
    Q_EMIT self->sourcesChanged();
}

qreal BlurBehind::saturation() const
{
    return m_saturation;
}

void BlurBehind::setSaturation(qreal saturation)
{
    if (qFuzzyCompare(m_saturation, saturation)) {
        return;
    }
    m_saturation = saturation;
    m_dirty = true;
    Q_EMIT saturationChanged();
    reapplySoon();
}

QRect BlurBehind::bounds() const
{
    return m_region.boundingRect();
}

bool BlurBehind::covers(int x, int y) const
{
    return m_region.contains(QPoint(x, y));
}

QRegion BlurBehind::roundedRect(const QRect &rect, qreal radius)
{
    if (rect.isEmpty()) {
        return {};
    }
    const qreal r = std::clamp(radius, 0.0, std::min(rect.width(), rect.height()) / 2.0);
    QRegion region;
    for (int row = 0; row < rect.height(); ++row) {
        // Distance of this pixel row's centre into the nearest rounded band, measured from the band's inner edge.
        const qreal centre = row + 0.5;
        const qreal fromEdge = std::min(centre, rect.height() - centre);
        qreal inset = 0;
        if (fromEdge < r) {
            const qreal dy = r - fromEdge;
            inset = r - std::sqrt(r * r - dy * dy);
        }
        const int x0 = rect.left() + int(std::lround(inset));
        const int width = rect.width() - 2 * int(std::lround(inset));
        if (width > 0) {
            region += QRect(x0, rect.top() + row, width, 1);
        }
    }
    return region;
}

// Appends to `found` the visible items under `item`, itself included, whose "glassBlur" property is true. It does
// not look inside an item that is hidden.
static void collectMarked(QQuickItem *item, QList<QQuickItem *> &found)
{
    if (!item->isVisible()) {
        return;
    }
    if (item->property("glassBlur").toBool()) {
        found.append(item);
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        collectMarked(child, found);
    }
}

// Uses the explicit sources when there are any, otherwise the items marked "glassBlur". Hidden, empty and nearly
// transparent items (0.01 or less in drawn opacity) add nothing.
QRegion BlurBehind::computeRegion() const
{
    QList<QQuickItem *> items;
    if (m_sources.isEmpty()) {
        if (m_window) {
            collectMarked(m_window->contentItem(), items);
        }
    } else {
        for (const QPointer<QQuickItem> &item : m_sources) {
            if (item) {
                items.append(item);
            }
        }
    }
    QRegion region;
    for (QQuickItem *item : std::as_const(items)) {
        if (!item->isVisible() || item->width() <= 0 || item->height() <= 0) {
            continue;
        }
        // A module that is fading out with its parent must not keep its glass, so use the opacity it is drawn with.
        qreal opacity = 1;
        for (const QQuickItem *step = item; step; step = step->parentItem()) {
            opacity *= step->opacity();
        }
        if (opacity <= 0.01) {
            continue;
        }
        const QRectF scene = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
        // The corner radius is in item units; the region is in window pixels.
        const qreal scale = scene.width() / item->width();
        region += roundedRect(scene.toRect(), item->property("radius").toReal() * scale);
    }
    return region;
}

void BlurBehind::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemSceneChange) {
        watchWindow(value.window);
    }
    QQuickItem::itemChange(change, value);
}

void BlurBehind::watchWindow(QQuickWindow *window)
{
    if (m_window == window) {
        return;
    }
    if (m_window) {
        m_window->removeEventFilter(this);
        disconnect(m_window, nullptr, this, nullptr);
        KWindowEffects::enableBlurBehind(m_window, false);
        KWindowEffects::enableBackgroundContrast(m_window, false);
    }
    m_window = window;
    m_region = {};
    m_dirty = true;
    if (m_window) {
        m_window->installEventFilter(this);
        // Every frame may move a module (open animation, a panel sliding in), and frames only happen on change.
        connect(m_window, &QQuickWindow::afterAnimating, this, &BlurBehind::refresh);
        m_window->update();
    }
}

bool BlurBehind::eventFilter(QObject *watched, QEvent *event)
{
    // PlasmaQuick::Dialog with NoBackground turns blur off on show, expose and resize (dialog.cpp, updateTheme);
    // this filter runs before its handler, so the region goes back on right after it.
    switch (event->type()) {
    case QEvent::Show:
    case QEvent::Expose:
    case QEvent::Resize:
    case QEvent::PlatformSurface:
        m_dirty = true;
        reapplySoon();
        break;
    default:
        break;
    }
    return QQuickItem::eventFilter(watched, event);
}

void BlurBehind::reapplySoon()
{
    QTimer::singleShot(0, this, &BlurBehind::refresh);
}

void BlurBehind::refresh()
{
    if (!m_window) {
        return;
    }
    const QRegion region = computeRegion();
    if (region != m_region) {
        m_region = region;
        m_dirty = true;
        Q_EMIT regionChanged();
    }
    if (!m_dirty || !m_window->isVisible()) {
        return;
    }
    m_dirty = false;
    const bool on = !m_region.isEmpty();
    KWindowEffects::enableBlurBehind(m_window, on, m_region);
    if (!qFuzzyCompare(m_saturation, 1.0)) {
        KWindowEffects::enableBackgroundContrast(m_window, on, 1.0, 1.0, m_saturation, m_region);
    }
}
