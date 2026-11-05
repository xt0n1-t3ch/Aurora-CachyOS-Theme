// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// BlurBehind asks KWin to blur what lies behind a set of rounded shapes of its window. The Control Center
// draws each module as its own piece of glass on a transparent window, the way macOS Tahoe does, so the blur
// must follow the modules instead of filling the window.
#pragma once

#include <QList>
#include <QPointer>
#include <QQmlListProperty>
#include <QQuickItem>
#include <QRegion>
#include <qqmlintegration.h>

// The QML item that keeps KWin's blur and contrast effects on its window in step with the shapes of its sources. It
// takes the region from the sources' positions and corner radii, re-sends it when it changes, and turns the effects
// off when it leaves the window. Use it on the GUI thread, as any Qt Quick item.
class BlurBehind : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    // Items to blur behind; an item's own "radius" property rounds its corners. Left empty, every item of the
    // window with a true "glassBlur" property is a source, so glass pieces can mark themselves.
    Q_PROPERTY(QQmlListProperty<QQuickItem> sources READ sources NOTIFY sourcesChanged)
    // Saturation of the blurred backdrop through KWin's background contrast effect; 1 leaves it alone.
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
    // Bounding box of the region last sent to KWin, in window coordinates.
    Q_PROPERTY(QRect bounds READ bounds NOTIFY regionChanged)

public:
    explicit BlurBehind(QQuickItem *parent = nullptr);
    ~BlurBehind() override;

    QQmlListProperty<QQuickItem> sources();
    qreal saturation() const;
    // Changing it sends the region again on the next event loop turn.
    void setSaturation(qreal saturation);
    QRect bounds() const;

    // Whether the window pixel at (x, y) is blurred.
    Q_INVOKABLE bool covers(int x, int y) const;

    // The pixels of a rounded rectangle, one row at a time; `radius` is clamped to half the shorter side. Empty for
    // an empty `rect`.
    static QRegion roundedRect(const QRect &rect, qreal radius);

Q_SIGNALS:
    void sourcesChanged();
    void saturationChanged();
    void regionChanged();

protected:
    // Follows the item into and out of a window.
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    // Watches the window; never consumes an event. Sends the region again when the window shows, resizes or gets a
    // new platform surface.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static void appendSource(QQmlListProperty<QQuickItem> *list, QQuickItem *item);
    static qsizetype sourceCount(QQmlListProperty<QQuickItem> *list);
    static QQuickItem *sourceAt(QQmlListProperty<QQuickItem> *list, qsizetype index);
    static void clearSources(QQmlListProperty<QQuickItem> *list);

    void watchWindow(QQuickWindow *window);
    // Recomputes the region and sends it to KWin when it, or the saturation, changed since the last send.
    void refresh();
    // Queues refresh() for the next event loop turn.
    void reapplySoon();
    // The region of the visible sources, in window coordinates.
    QRegion computeRegion() const;

    QList<QPointer<QQuickItem>> m_sources;
    QPointer<QQuickWindow> m_window;
    QRegion m_region;
    qreal m_saturation = 1.0;
    bool m_dirty = true; // Whether KWin has to be told again: the region, the saturation or the window changed.
};
