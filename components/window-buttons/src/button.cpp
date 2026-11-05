// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Draws a window button and keeps it in step with what the window can do (minimize, maximize, close).

#include "button.h"

#include "decoration.h"
#include "glass.h"

#include <KDecoration3/DecoratedWindow>

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace aurora {

namespace {

using KDecoration3::DecorationButtonType;

constexpr int HOVER_FADE_MS = 120; // Length of the hover fade in milliseconds.
const QColor CLOSE_HOVER{255, 69, 58};
const QColor CLOSE_PRESSED{214, 52, 44};

} // namespace

Button::Button(DecorationButtonType type, Decoration *decoration, QObject *parent)
    : KDecoration3::DecorationButton(type, decoration, parent)
    , m_hover(new QVariantAnimation(this))
{
    m_hover->setStartValue(0.0);
    m_hover->setEndValue(1.0);
    m_hover->setDuration(HOVER_FADE_MS);
    m_hover->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hover, &QVariantAnimation::valueChanged, this, [this] { update(); });
    connect(this, &KDecoration3::DecorationButton::hoveredChanged, this, [this](bool hovered) {
        m_hover->setDirection(hovered ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
        if (m_hover->state() != QAbstractAnimation::Running) {
            m_hover->start();
        }
    });
}

Button::Button(QObject *parent, const QVariantList &args)
    : Button(args.at(0).value<DecorationButtonType>(), args.at(1).value<Decoration *>(), parent)
{
    setGeometry(QRectF(0, 0, SEGMENT_WIDTH, CAPSULE_HEIGHT));
}

KDecoration3::DecorationButton *Button::create(DecorationButtonType type, KDecoration3::Decoration *decoration, QObject *parent)
{
    auto *owner = qobject_cast<Decoration *>(decoration);
    // The capsule has segments only for the three window buttons.
    const bool windowButton = type == DecorationButtonType::Minimize || type == DecorationButtonType::Maximize || type == DecorationButtonType::Close;
    if (!owner || !windowButton) {
        return nullptr;
    }
    auto *window = owner->window();
    auto *button = new Button(type, owner, parent);
    switch (type) {
    case DecorationButtonType::Minimize:
        button->setVisible(window->isMinimizeable());
        connect(window, &KDecoration3::DecoratedWindow::minimizeableChanged, button, &Button::setVisible);
        break;
    case DecorationButtonType::Maximize:
        button->setVisible(window->isMaximizeable());
        connect(window, &KDecoration3::DecoratedWindow::maximizeableChanged, button, &Button::setVisible);
        break;
    default:
        button->setVisible(window->isCloseable());
        connect(window, &KDecoration3::DecoratedWindow::closeableChanged, button, &Button::setVisible);
        break;
    }
    connect(button, &KDecoration3::DecorationButton::visibilityChanged, owner, &Decoration::relayoutSoon);
    return button;
}

void Button::setVisualRect(const QRectF &rect)
{
    m_visualRect = rect;
}

void Button::paint(QPainter *painter, const QRectF &repaintArea)
{
    auto *owner = qobject_cast<Decoration *>(decoration());
    const QRectF segment = m_visualRect.isValid() ? m_visualRect : geometry();
    if (!owner || !isVisible() || !segment.intersects(repaintArea)) {
        return;
    }
    const bool close = type() == DecorationButtonType::Close;
    const bool onDark = owner->isTitleBarDark();
    const qreal hover = isPressed() ? 1.0 : m_hover->currentValue().toReal();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (hover > 0) {
        // The same inset pill as a hovered File Explorer toolbar button, red for close.
        const QRectF pill = segment.adjusted(2, 3, -2, -3);
        QColor fill = onDark ? QColor(255, 255, 255, isPressed() ? 50 : 30) : QColor(0, 0, 0, isPressed() ? 36 : 20);
        if (close) {
            fill = isPressed() ? CLOSE_PRESSED : CLOSE_HOVER;
        }
        fill.setAlphaF(fill.alphaF() * hover);
        QPainterPath path;
        path.addRoundedRect(pill, pill.height() / 2, pill.height() / 2);
        painter->fillPath(path, fill);
    }

    QColor ink = owner->glyphColor();
    if (close && hover > 0) {
        // Fade the stroke to white as the red pill fades in, so it stays readable on red.
        const QColor white(255, 255, 255);
        ink = QColor::fromRgbF(ink.redF() + (white.redF() - ink.redF()) * hover, ink.greenF() + (white.greenF() - ink.greenF()) * hover,
                               ink.blueF() + (white.blueF() - ink.blueF()) * hover, ink.alphaF() + (1.0 - ink.alphaF()) * hover);
    }
    Glyph glyph = Glyph::Close;
    if (type() == DecorationButtonType::Minimize) {
        glyph = Glyph::Minimize;
    } else if (type() == DecorationButtonType::Maximize) {
        glyph = isChecked() ? Glyph::Restore : Glyph::Maximize;
    }
    paintGlyph(*painter, glyph, segment.center(), ink);
    painter->restore();
}

} // namespace aurora
