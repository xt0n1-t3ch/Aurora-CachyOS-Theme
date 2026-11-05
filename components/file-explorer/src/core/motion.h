// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// How File Explorer's own transitions move: timed by the desktop's animation speed (kdeglobals [KDE]
// AnimationDurationFactor, the value Plasma, KWin and Qt Quick apps use), with curves that slow down at the end.

#pragma once

#include <QObject>

class QVariantAnimation;

namespace fe::motion {

/// `baseMs` at the animation speed `speed`: 1 is Plasma's designed timing, 0 turns animations off.
[[nodiscard]] int scaled(int baseMs, double speed);

/// The desktop's animation speed; File Explorer sets it from kdeglobals at start and when it changes.
[[nodiscard]] double speed();
void setSpeed(double speed);

/// A hover that fades in and out over `baseMs` at the animation speed, instead of switching. progress() runs from
/// 0 (not hovered) to 1 (hovered); a widget repaints on changed() and draws its hover at that strength.
class HoverFade : public QObject
{
    Q_OBJECT

public:
    explicit HoverFade(int baseMs, double speed, QObject *parent = nullptr);
    /// At the desktop's animation speed (speed()).
    explicit HoverFade(int baseMs, QObject *parent = nullptr);

    void setHovered(bool hovered);
    /// Back to not hovered at once, without a fade: the hover moved to another segment of the same widget.
    void reset();
    [[nodiscard]] qreal progress() const;

Q_SIGNALS:
    void changed();

private:
    int m_baseMs;
    double m_speed;
    qreal m_progress = 0.0;
    QVariantAnimation *m_animation;
};

} // namespace fe::motion
