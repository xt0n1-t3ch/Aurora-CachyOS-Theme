// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The QML element that turns on the pointing hand for the Qt Quick surfaces of the process it is created in.

#pragma once

#include <QObject>
#include <QQmlEngine>

class QQuickItem;

// The pointing hand over everything clickable in plasmashell, as Aurora shows it on every clickable surface.
// While one PointerHints lives, the item a resting pointer is over gets the hand, once, the way a cursorShape would
// set it, when it takes left clicks: a Qt Quick Controls button (not a menu item), an item with an enabled TapHandler
// for the left button (the Dock's tasks, Plasma 6.8 Task.qml:420), or a hover-enabled MouseArea (tray icons). The
// first such item from the one under the pointer up its parents wins. Items with a cursor of their own keep it, a
// MouseArea without hover (a wheel or press catcher) keeps the arrow, and the desktop's own window is left alone.
// All instances share one application-wide event filter, which lives as long as any of them does.
class PointerHints : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit PointerHints(QObject *parent = nullptr);
    ~PointerHints() override;

    // The cursor shape an item shows (Qt::CursorShape); Qt::ArrowCursor for a null item. For tests and debugging.
    Q_INVOKABLE int cursorShapeOf(QQuickItem *item) const;
};
