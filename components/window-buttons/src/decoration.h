// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The Aurora window decoration for KWin: the title bar, its shadow and its button capsule.

#pragma once

#include "titlelayout.h"

#include <KDecoration3/Decoration>

#include <QVariant>

#include <memory>

namespace KDecoration3 {
class DecorationButtonGroup;
class DecorationShadow;
}

namespace aurora {

// The Aurora window decoration: a Liquid Glass title bar in the window's own color, its title
// centred as on macOS, the File Explorer's button capsule in Windows order at the right, rounded
// corners, a fine light edge and a deep soft shadow.
class Decoration : public KDecoration3::Decoration
{
    Q_OBJECT

public:
    explicit Decoration(QObject *parent = nullptr, const QVariantList &args = {});

    // Creates the button group, follows the window and the settings, and lays the title bar out. Returns true.
    bool init() override;
    // Draws the title bar, its caption and the capsule, then the buttons. Draws nothing when `repaintRegion`
    // misses the title bar.
    void paint(QPainter *painter, const QRectF &repaintRegion) override;

    // Whether the window's own background is dark, so the title bar and the glyphs use light ink.
    [[nodiscard]] bool isTitleBarDark() const;
    // The stroke color of the button glyphs: the window's text color, faded when the window is inactive.
    [[nodiscard]] QColor glyphColor() const;

    // Lays the title bar out again once the current event is handled (a button appeared or left).
    void relayoutSoon();

private:
    [[nodiscard]] bool isEdgeToEdge() const;
    [[nodiscard]] QColor titleBarColor() const;
    void relayout();
    void updateShadow();

    KDecoration3::DecorationButtonGroup *m_buttons = nullptr;
    TitleLayout m_layout;
    bool m_relayoutPending = false; // Set while a queued relayout waits, so repeated requests run it once.
};

} // namespace aurora
