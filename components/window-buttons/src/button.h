// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// One segment of the title bar capsule: the minimize, maximize or close button of the Aurora decoration.

#pragma once

#include <KDecoration3/DecorationButton>

#include <QVariant>

class QVariantAnimation;

namespace aurora {

class Decoration;

// One segment of the title bar capsule: minimize, maximize or close. It draws the File Explorer's
// strokes, a soft pill under the pointer, and red for close.
class Button : public KDecoration3::DecorationButton
{
    Q_OBJECT

public:
    // Creates a segment of the given type for `decoration`. The hover animation is a child of the button.
    Button(KDecoration3::DecorationButtonType type, Decoration *decoration, QObject *parent = nullptr);
    // The constructor KPluginFactory calls, for the window decoration settings preview. `args` holds the
    // button type, then the Decoration, in that order; the segment gets the default capsule size.
    Button(QObject *parent, const QVariantList &args);

    // The button factory of the capsule's group. Returns nullptr for the button types the capsule has no
    // segment for, or when `decoration` is not an Aurora decoration. The button follows whether the window
    // can be minimized, maximized or closed, and asks the decoration to lay out again when that changes.
    static KDecoration3::DecorationButton *create(KDecoration3::DecorationButtonType type, KDecoration3::Decoration *decoration,
                                                  QObject *parent);

    // Where the segment is drawn, in decoration coordinates; the geometry may reach further, to the
    // screen edge of a maximized window. Without a valid rectangle, the geometry is drawn instead.
    void setVisualRect(const QRectF &rect);

    // Draws the hover pill and the stroke inside the visual rectangle. Draws nothing when the button is
    // hidden or the rectangle lies outside `repaintArea`.
    void paint(QPainter *painter, const QRectF &repaintArea) override;

private:
    QRectF m_visualRect;
    QVariantAnimation *m_hover; // Fades from 0 (idle) to 1 (hovered).
};

} // namespace aurora
