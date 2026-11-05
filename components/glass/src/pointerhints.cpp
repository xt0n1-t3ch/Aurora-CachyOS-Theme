// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Finds the Qt Quick item under a resting pointer and sets the pointing hand on it when it takes left clicks.

#include "pointerhints.h"

#include <QCoreApplication>
#include <QCursor>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>

namespace {

// Whether the object's class, or one of its base classes, has the given name. The Qt Quick classes tested here are
// private to Qt, so they are matched by name.
bool isA(const QObject *object, const char *className)
{
    for (const QMetaObject *meta = object->metaObject(); meta; meta = meta->superClass()) {
        if (qstrcmp(meta->className(), className) == 0) {
            return true;
        }
    }
    return false;
}

// Whether an enabled, visible item reacts to a left click, by the rules in the PointerHints comment.
bool takesLeftClicks(const QQuickItem *item)
{
    if (!item->isEnabled() || !item->isVisible()) {
        return false;
    }
    if (isA(item, "QQuickAbstractButton")) {
        // Menus keep the arrow, as on macOS and Windows.
        return !isA(item, "QQuickMenuItem");
    }
    if (isA(item, "QQuickMouseArea")) {
        return item->property("hoverEnabled").toBool() && (item->property("acceptedButtons").toInt() & Qt::LeftButton);
    }
    // Pointer handlers are the item's QObject children (QQuickItemPrivate::addPointerHandler parents them there).
    for (const QObject *child : item->children()) {
        if (isA(child, "QQuickTapHandler") && child->property("enabled").toBool()
            && (child->property("acceptedButtons").toInt() & Qt::LeftButton)) {
            return true;
        }
    }
    return false;
}

// One filter for the whole application, however many PointerHints there are; `users` counts them. It watches mouse
// moves of Qt Quick windows and never consumes an event.
class HandFilter : public QObject
{
public:
    int users = 0;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::MouseMove) {
            return false;
        }
        auto *window = qobject_cast<QQuickWindow *>(watched);
        const auto *mouse = static_cast<const QMouseEvent *>(event);
        // The desktop's own view (plasmashell's DesktopView) keeps the arrow: a click there selects or opens menus.
        if (!window || mouse->buttons() != Qt::NoButton || qstrcmp(window->metaObject()->className(), "DesktopView") == 0) {
            return false;
        }
        // A pointer moving at 1000 Hz needs no answer faster than about a frame of a 165 Hz screen.
        if (m_lastWindow == window && m_clock.isValid() && m_clock.elapsed() < 6) {
            return false;
        }
        m_lastWindow = window;
        m_clock.start();
        // Still over the item that got the hand last time: nothing to look up.
        if (m_lastHit && m_lastHit->window() == window && m_lastHit->isVisible() && m_lastHit->contains(m_lastHit->mapFromScene(mouse->position()))) {
            return false;
        }
        m_lastHit = hint(window, mouse->position());
        return false;
    }

private:
    // Gives the hand to the item under `pos` when it still shows the arrow. Returns that item, or nullptr.
    static QQuickItem *hint(QQuickWindow *window, QPointF pos)
    {
        // POINTER_DEBUG=1 logs how long each lookup takes.
        static const bool timing = qEnvironmentVariableIntValue("POINTER_DEBUG") == 1;
        QElapsedTimer spent;
        if (timing) {
            spent.start();
        }
        QQuickItem *hit = itemAt(window->contentItem(), pos, 0);
        if (timing) {
            qInfo("GGHINT %lld us in %s", static_cast<long long>(spent.nsecsElapsed() / 1000), window->metaObject()->className());
        }
        if (hit && hit->cursor().shape() == Qt::ArrowCursor) {
            giveHand(hit, window);
        }
        return hit;
    }

    // The topmost item under `pos` that takes a left click or has a cursor of its own, searched the way Qt Quick
    // delivers a press: children before their parent, the higher z first, so a decoration drawn over a click area
    // (the tray's icon over its MouseArea, AbstractItem.qml) does not hide it. The search stops at `depth` 96.
    static QQuickItem *itemAt(QQuickItem *item, QPointF pos, int depth)
    {
        if (!item->isVisible() || depth > 96) {
            return nullptr;
        }
        const bool inside = item->contains(item->mapFromScene(pos));
        if (!inside && item->clip()) {
            return nullptr;
        }
        QList<QQuickItem *> children = item->childItems();
        std::stable_sort(children.begin(), children.end(), [](const QQuickItem *a, const QQuickItem *b) { return a->z() < b->z(); });
        for (auto child = children.crbegin(); child != children.crend(); ++child) {
            if (QQuickItem *hit = itemAt(*child, pos, depth + 1)) {
                return hit;
            }
        }
        // The panel or the desktop itself never shows the hand: a click on the empty bar does nothing.
        if (!inside || isA(item, "ContainmentItem")) {
            return nullptr;
        }
        if (item->cursor().shape() != Qt::ArrowCursor || takesLeftClicks(item)) {
            return item;
        }
        return nullptr;
    }

    // Sets the pointing hand cursor on `item`; `window` only names the surface in the debug log.
    static void giveHand(QQuickItem *item, const QQuickWindow *window)
    {
        item->setCursor(Qt::PointingHandCursor);
        // POINTER_DEBUG=1 names each item that got the hand, to check a surface without seeing the pointer.
        static const bool debug = qEnvironmentVariableIntValue("POINTER_DEBUG") == 1;
        if (debug) {
            qInfo("GGHAND %s in %s", item->metaObject()->className(), window->metaObject()->className());
        }
    }

    QQuickWindow *m_lastWindow = nullptr;
    QPointer<QQuickItem> m_lastHit;
    QElapsedTimer m_clock;
};

// The filter shared by every PointerHints; null while none exists.
HandFilter *s_filter = nullptr;

} // namespace

PointerHints::PointerHints(QObject *parent)
    : QObject(parent)
{
    if (!s_filter) {
        s_filter = new HandFilter;
        QCoreApplication::instance()->installEventFilter(s_filter);
    }
    ++s_filter->users;
}

PointerHints::~PointerHints()
{
    if (s_filter && --s_filter->users == 0) {
        QCoreApplication::instance()->removeEventFilter(s_filter);
        delete s_filter;
        s_filter = nullptr;
    }
}

int PointerHints::cursorShapeOf(QQuickItem *item) const
{
    return item ? item->cursor().shape() : Qt::ArrowCursor;
}
