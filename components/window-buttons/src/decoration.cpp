// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Lays the Aurora title bar out, paints it, and gives it its shadow, borders and blur region.

#include "decoration.h"

#include "button.h"
#include "glass.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationButtonGroup>
#include <KDecoration3/DecorationSettings>
#include <KDecoration3/DecorationShadow>
#include <KDecoration3/ScaleHelpers>
#include <KPluginFactory>

#include <QLinearGradient>
#include <QPainter>
#include <QTimer>

K_PLUGIN_FACTORY_WITH_JSON(WindowButtonsFactory, "decoration.json", registerPlugin<aurora::Decoration>();
                           registerPlugin<aurora::Button>();)

namespace aurora {

namespace {

// Shadow measures in pixels, and the shadow's opacity from 0 to 255.
constexpr int SHADOW_EXTENT = 64;
constexpr int SHADOW_OFFSET_Y = 14;
constexpr int SHADOW_BLUR = 15;
constexpr int SHADOW_ALPHA_ACTIVE = 150;
constexpr int SHADOW_ALPHA_INACTIVE = 90;
// Width in pixels of the invisible band around a floating window that resizes it but is not drawn.
constexpr qreal RESIZE_BAND = 8;
// Opacity from 0 to 1 of the title bar color when the compositor supports an alpha channel.
constexpr qreal GLASS_ALPHA_ACTIVE = 0.86;
constexpr qreal GLASS_ALPHA_INACTIVE = 0.80;

// Builds the shadow image for windows with CORNER_RADIUS corners; `alpha` (0 to 255) is the shadow's opacity
// before it is blurred.
std::shared_ptr<KDecoration3::DecorationShadow> makeShadow(int alpha)
{
    const int radius = static_cast<int>(CORNER_RADIUS);
    auto shadow = std::make_shared<KDecoration3::DecorationShadow>();
    shadow->setShadow(shadowImage(radius, SHADOW_EXTENT, SHADOW_OFFSET_Y, SHADOW_BLUR, alpha));
    shadow->setPadding(QMarginsF(SHADOW_EXTENT, SHADOW_EXTENT, SHADOW_EXTENT, SHADOW_EXTENT));
    // The single pixel KWin stretches along each side; the rest of the image is the eight corner and edge tiles.
    shadow->setInnerShadowRect(QRectF(SHADOW_EXTENT + radius, SHADOW_EXTENT + radius, 1, 1));
    return shadow;
}

} // namespace

Decoration::Decoration(QObject *parent, const QVariantList &args)
    : KDecoration3::Decoration(parent, args)
{
}

bool Decoration::init()
{
    auto *w = window();
    m_buttons = new KDecoration3::DecorationButtonGroup(KDecoration3::DecorationButtonGroup::Position::Right, this, &Button::create);

    const auto relayoutNow = [this] { relayout(); };
    connect(w, &KDecoration3::DecoratedWindow::widthChanged, this, relayoutNow);
    connect(w, &KDecoration3::DecoratedWindow::maximizedChanged, this, relayoutNow);
    connect(w, &KDecoration3::DecoratedWindow::shadedChanged, this, relayoutNow);
    connect(w, &KDecoration3::DecoratedWindow::paletteChanged, this, relayoutNow);
    connect(w, &KDecoration3::DecoratedWindow::activeChanged, this, [this] {
        updateShadow();
        relayout();
    });
    connect(w, &KDecoration3::DecoratedWindow::captionChanged, this, [this] { update(titleBar()); });
    const auto s = settings();
    connect(s.get(), &KDecoration3::DecorationSettings::reconfigured, this, &Decoration::relayoutSoon);
    connect(s.get(), &KDecoration3::DecorationSettings::decorationButtonsRightChanged, this, &Decoration::relayoutSoon);
    connect(s.get(), &KDecoration3::DecorationSettings::fontChanged, this, [this] { update(); });

    relayout();
    updateShadow();
    return true;
}

bool Decoration::isEdgeToEdge() const
{
    return window()->isMaximized();
}

bool Decoration::isTitleBarDark() const
{
    return isDark(window()->palette().color(QPalette::Window));
}

QColor Decoration::titleBarColor() const
{
    const bool active = window()->isActive();
    QColor color = window()->palette().color(active ? QPalette::Active : QPalette::Inactive, QPalette::Window);
    if (settings()->isAlphaChannelSupported()) {
        color.setAlphaF(active ? GLASS_ALPHA_ACTIVE : GLASS_ALPHA_INACTIVE);
    }
    return color;
}

QColor Decoration::glyphColor() const
{
    QColor color = window()->palette().color(QPalette::Active, QPalette::WindowText);
    color.setAlphaF(window()->isActive() ? 0.92 : 0.36);
    return color;
}

void Decoration::relayoutSoon()
{
    if (!m_relayoutPending) {
        m_relayoutPending = true;
        QTimer::singleShot(0, this, &Decoration::relayout);
    }
}

void Decoration::relayout()
{
    m_relayoutPending = false;
    const bool edgeToEdge = isEdgeToEdge();
    const qreal width = window()->width();
    setBorders(QMarginsF(0, TITLE_BAR_HEIGHT, 0, 0));
    setResizeOnlyBorders(edgeToEdge ? QMarginsF() : QMarginsF(RESIZE_BAND, RESIZE_BAND / 2, RESIZE_BAND, RESIZE_BAND));
    setTitleBar(QRectF(0, 0, width, TITLE_BAR_HEIGHT));

    QList<Button *> visible;
    const auto buttons = m_buttons->buttons();
    for (KDecoration3::DecorationButton *button : buttons) {
        if (button->isVisible()) {
            visible.append(static_cast<Button *>(button));
        }
    }
    m_layout = layoutTitleBar(width, TITLE_BAR_HEIGHT, static_cast<int>(visible.size()), edgeToEdge);
    // The group places its buttons one after the other from its position, so touching hit areas
    // reproduce the layout exactly.
    m_buttons->setSpacing(0);
    for (qsizetype i = 0; i < visible.size(); ++i) {
        visible.at(i)->setGeometry(QRectF(QPointF(0, 0), m_layout.hitAreas.at(i).size()));
        visible.at(i)->setVisualRect(m_layout.buttons.at(i));
    }
    if (!visible.isEmpty()) {
        m_buttons->setPos(m_layout.hitAreas.constFirst().topLeft());
    }

    // KWin rounds the window's bottom corners; the title bar draws its own rounded top.
    const qreal radius = edgeToEdge ? 0 : CORNER_RADIUS;
    setBorderRadius(KDecoration3::BorderRadius(0, 0, radius, radius));
    if (edgeToEdge) {
        setBorderOutline(KDecoration3::BorderOutline());
    } else {
        const qreal scale = window()->nextScale();
        const qreal thickness = std::max(KDecoration3::pixelSize(scale), KDecoration3::snapToPixelGrid(1, scale));
        const QColor edge = isTitleBarDark() ? QColor(255, 255, 255, window()->isActive() ? 30 : 18) : QColor(0, 0, 0, 40);
        setBorderOutline(KDecoration3::BorderOutline(thickness, edge, KDecoration3::BorderRadius(radius, radius, radius, radius)));
    }

    const QPainterPath glass = titleBarPath(QRectF(0, 0, width, TITLE_BAR_HEIGHT), radius, window()->isShaded());
    setBlurRegion(QRegion(glass.toFillPolygon().toPolygon()));
    update();
}

void Decoration::updateShadow()
{
    // One image per state for every window, as Breeze does.
    static std::shared_ptr<KDecoration3::DecorationShadow> activeShadow;
    static std::shared_ptr<KDecoration3::DecorationShadow> inactiveShadow;
    const bool active = window()->isActive();
    auto &shadow = active ? activeShadow : inactiveShadow;
    if (!shadow) {
        shadow = makeShadow(active ? SHADOW_ALPHA_ACTIVE : SHADOW_ALPHA_INACTIVE);
    }
    setShadow(shadow);
}

void Decoration::paint(QPainter *painter, const QRectF &repaintRegion)
{
    const QRectF bar(0, 0, size().width(), TITLE_BAR_HEIGHT);
    if (!bar.intersects(repaintRegion)) {
        return;
    }
    const bool active = window()->isActive();
    const bool dark = isTitleBarDark();
    const QPainterPath shape = titleBarPath(bar, isEdgeToEdge() ? 0 : CORNER_RADIUS, window()->isShaded());

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillPath(shape, titleBarColor());
    // The light a Liquid Glass surface catches along its top.
    QLinearGradient sheen(bar.topLeft(), bar.bottomLeft());
    sheen.setColorAt(0.0, QColor(255, 255, 255, dark ? 14 : 40));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter->fillPath(shape, sheen);
    if (!window()->isShaded()) {
        // The hairline macOS draws where a title bar meets the window.
        painter->setPen(QPen(dark ? QColor(0, 0, 0, 70) : QColor(0, 0, 0, 28), 1.0));
        painter->drawLine(QPointF(0, bar.bottom() - 0.5), QPointF(bar.right(), bar.bottom() - 0.5));
    }

    QFont font = settings()->font();
    font.setWeight(QFont::DemiBold);
    painter->setFont(font);
    const QFontMetricsF metrics(font);
    const QString caption = window()->caption();
    const QRectF area = captionRect(bar.width(), TITLE_BAR_HEIGHT, metrics.horizontalAdvance(caption), m_layout.capsule);
    QColor text = window()->palette().color(QPalette::Active, QPalette::WindowText);
    text.setAlphaF(active ? 0.9 : 0.45);
    painter->setPen(text);
    painter->drawText(area, Qt::AlignCenter | Qt::TextSingleLine, metrics.elidedText(caption, Qt::ElideMiddle, area.width()));

    if (!m_layout.capsule.isEmpty()) {
        paintCapsule(*painter, m_layout.capsule, dark, active);
    }
    painter->restore();
    m_buttons->paint(painter, repaintRegion);
}

} // namespace aurora

#include "decoration.moc"
