// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.

#include "core/motion.h"

#include <algorithm>
#include <cmath>

#include <QEasingCurve>
#include <QVariantAnimation>

namespace fe::motion {

namespace {
double s_speed = 1.0;
}

int scaled(int baseMs, double speed)
{
    return speed > 0.0 ? static_cast<int>(std::lround(baseMs * speed)) : 0;
}

double speed()
{
    return s_speed;
}

void setSpeed(double speed)
{
    s_speed = speed;
}

HoverFade::HoverFade(int baseMs, double speed, QObject *parent)
    : QObject(parent)
    , m_baseMs(baseMs)
    , m_speed(speed)
    , m_animation(new QVariantAnimation(this))
{
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_progress = value.toReal();
        Q_EMIT changed();
    });
}

HoverFade::HoverFade(int baseMs, QObject *parent)
    : HoverFade(baseMs, motion::speed(), parent)
{
}

void HoverFade::setHovered(bool hovered)
{
    const qreal target = hovered ? 1.0 : 0.0;
    m_animation->stop();
    const int full = scaled(m_baseMs, m_speed);
    if (full == 0 || qFuzzyCompare(m_progress + 1.0, target + 1.0)) {
        m_progress = target;
        Q_EMIT changed();
        return;
    }
    // A turn halfway takes only the part of the time that is left, from where the fade is now.
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(target);
    m_animation->setDuration(std::max(1, static_cast<int>(std::lround(full * std::abs(target - m_progress)))));
    m_animation->start();
}

void HoverFade::reset()
{
    m_animation->stop();
    m_progress = 0.0;
    Q_EMIT changed();
}

qreal HoverFade::progress() const
{
    return m_progress;
}

} // namespace fe::motion
