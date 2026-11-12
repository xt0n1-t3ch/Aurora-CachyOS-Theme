// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements finderstyle.h. Everything it does not draw itself it passes to the platform style underneath.

#include "finderstyle.h"

#include "core/format.h"
#include "shadow.h"
#include "style.h"
#include "core/motion.h"

#include <KWindowEffects>

#include <QAbstractButton>
#include <QComboBox>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyleOption>
#include <QTimer>
#include <QWindow>

namespace fe {

namespace {

constexpr int TOOLTIP_RADIUS = 8;
constexpr int MENU_TEXT_LEFT = 14;
constexpr int MENU_CHECK_WIDTH = 18;
constexpr int MENU_ICON_WIDTH = 24;
constexpr int MENU_ICON_SIZE = 16;
constexpr int MENU_ARROW_WIDTH = 22;
constexpr int MENU_SHORTCUT_GAP = 28;
constexpr int MENU_MIN_WIDTH = 180;
constexpr int SCROLLBAR_EXTENT = 12;
constexpr int SCROLLBAR_MIN_SLIDER = 36;
// Taller than any list row: an item this tall is an icon tile (a thumbnail with its name under it).
constexpr int TILE_MIN_HEIGHT = 44;
const QColor TOOLTIP_FILL{44, 44, 48, 225};

/// Breeze's shadow helper skips windows with this property (breezeshadowhelper.cpp, acceptWidget);
/// the popups get a shadow shaped for their own, rounder corners instead.
constexpr char SKIP_SHADOW_PROPERTY[] = "_KDE_NET_WM_SKIP_SHADOW";

// The popups this style turns into glass: menus and tooltips (QTipLabel is Qt's tooltip widget).
bool isGlassPopup(const QWidget *widget)
{
    return qobject_cast<const QMenu *>(widget) || widget->inherits("QTipLabel");
}

int popupRadius(const QWidget *widget)
{
    return qobject_cast<const QMenu *>(widget) ? style::MENU_RADIUS : TOOLTIP_RADIUS;
}

// Asks KWin to blur behind the popup's rounded body.
void blurPopup(QWidget *popup)
{
    if (!popup->windowHandle()) {
        return;
    }
    const int radius = popupRadius(popup);
    QPainterPath body;
    body.addRoundedRect(QRectF(popup->rect()), radius, radius);
    KWindowEffects::enableBlurBehind(popup->windowHandle(), true, QRegion(body.toFillPolygon().toPolygon()));
}

// The text without its '&' mnemonic markers.
QString withoutMnemonic(const QString &text)
{
    QString plain;
    plain.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == u'&' && i + 1 < text.size()) {
            ++i;
        }
        plain += text.at(i);
    }
    return plain;
}

struct MenuText {
    QString label;
    QString shortcut;
};

/// QMenu hands the item text as "label\tshortcut".
MenuText splitMenuText(const QString &text)
{
    const qsizetype tab = text.indexOf(u'\t');
    if (tab < 0) {
        return {text, {}};
    }
    return {text.left(tab), shortcutLabel(text.mid(tab + 1))};
}

// Strokes a chevron pointing up or right; any other direction points down.
void drawChevron(QPainter *painter, QPointF center, qreal size, Qt::ArrowType direction, const QColor &color)
{
    QPainterPath path;
    const qreal h = size / 2.0;
    const qreal w = size / 4.0;
    switch (direction) {
    case Qt::RightArrow:
        path.moveTo(center + QPointF(-w, -h));
        path.lineTo(center + QPointF(w, 0));
        path.lineTo(center + QPointF(-w, h));
        break;
    case Qt::UpArrow:
        path.moveTo(center + QPointF(-h, w));
        path.lineTo(center + QPointF(0, -w));
        path.lineTo(center + QPointF(h, w));
        break;
    default:
        path.moveTo(center + QPointF(-h, -w));
        path.lineTo(center + QPointF(0, w));
        path.lineTo(center + QPointF(h, -w));
        break;
    }
    painter->setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}

void drawCheck(QPainter *painter, const QRectF &box, const QColor &color)
{
    QPainterPath path;
    path.moveTo(box.left(), box.top() + box.height() * 0.55);
    path.lineTo(box.left() + box.width() * 0.38, box.bottom());
    path.lineTo(box.right(), box.top());
    painter->setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}

// Draws a menu row. A separator with text is a section title; one without is a hairline.
void drawMenuItem(const QStyleOptionMenuItem &item, QPainter *painter)
{
    const QRect rect = item.rect;
    if (item.menuItemType == QStyleOptionMenuItem::Separator) {
        if (!item.text.isEmpty()) {
            painter->setFont(style::uiFont(11, QFont::DemiBold));
            painter->setPen(style::TEXT_SECONDARY);
            painter->drawText(rect.adjusted(MENU_TEXT_LEFT, 0, -MENU_TEXT_LEFT, 0), Qt::AlignLeft | Qt::AlignVCenter,
                              withoutMnemonic(item.text));
            return;
        }
        const qreal y = rect.center().y() + 0.5;
        painter->setPen(QPen(style::MENU_SEPARATOR, 1.0));
        painter->drawLine(QPointF(rect.left() + 12, y), QPointF(rect.right() - 11, y));
        return;
    }

    const bool enabled = item.state & QStyle::State_Enabled;
    const bool selected = enabled && (item.state & QStyle::State_Selected);
    if (selected) {
        QPainterPath pill;
        pill.addRoundedRect(QRectF(rect).adjusted(5, 0, -5, 0), 6, 6);
        painter->fillPath(pill, style::ACCENT);
    }
    const QColor text = !enabled ? style::TEXT_TERTIARY : selected ? QColor(Qt::white) : style::TEXT;
    const QColor secondary = !enabled ? style::TEXT_TERTIARY : selected ? QColor(255, 255, 255, 210) : style::TEXT_SECONDARY;

    int x = rect.left() + MENU_TEXT_LEFT;
    if (item.menuHasCheckableItems) {
        if (item.checked) {
            const QRectF box(x, rect.center().y() - 4.0, 10.0, 8.5);
            drawCheck(painter, box, text);
        }
        x += MENU_CHECK_WIDTH;
    }
    if (item.maxIconWidth > 0) {
        if (!item.icon.isNull()) {
            const QIcon::Mode mode = enabled ? QIcon::Normal : QIcon::Disabled;
            const QPixmap icon = item.icon.pixmap(QSize(MENU_ICON_SIZE, MENU_ICON_SIZE), painter->device()->devicePixelRatioF(), mode);
            painter->drawPixmap(QPoint(x, rect.top() + (rect.height() - MENU_ICON_SIZE) / 2), icon);
        }
        x += MENU_ICON_WIDTH;
    }

    const MenuText parts = splitMenuText(item.text);
    const int right = rect.right() - (item.menuItemType == QStyleOptionMenuItem::SubMenu ? MENU_ARROW_WIDTH : MENU_TEXT_LEFT);
    const QRect textRect(x, rect.top(), right - x, rect.height());
    QFont font = item.font;
    if (item.menuItemType == QStyleOptionMenuItem::DefaultItem) {
        font.setWeight(QFont::DemiBold);
    }
    painter->setFont(font);
    painter->setPen(text);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextHideMnemonic | Qt::TextSingleLine, parts.label);
    if (!parts.shortcut.isEmpty()) {
        painter->setPen(secondary);
        painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, parts.shortcut);
    }
    if (item.menuItemType == QStyleOptionMenuItem::SubMenu) {
        drawChevron(painter, QPointF(rect.right() - 14, rect.center().y() + 0.5), 8.0, Qt::RightArrow, secondary);
    }
}

// The size of a menu row, wide enough for its label, icon column, shortcut and submenu arrow.
QSize menuItemSize(const QStyleOptionMenuItem &item, const QSize &contents)
{
    if (item.menuItemType == QStyleOptionMenuItem::Separator) {
        return {contents.width(), item.text.isEmpty() ? style::MENU_SEPARATOR_HEIGHT : style::MENU_ITEM_HEIGHT};
    }
    const QFontMetrics metrics(item.font);
    const MenuText parts = splitMenuText(item.text);
    int width = MENU_TEXT_LEFT + metrics.horizontalAdvance(withoutMnemonic(parts.label)) + MENU_TEXT_LEFT;
    if (item.menuHasCheckableItems) {
        width += MENU_CHECK_WIDTH;
    }
    if (item.maxIconWidth > 0) {
        width += MENU_ICON_WIDTH;
    }
    if (!parts.shortcut.isEmpty()) {
        width += MENU_SHORTCUT_GAP + metrics.horizontalAdvance(parts.shortcut);
    }
    if (item.menuItemType == QStyleOptionMenuItem::SubMenu) {
        width += MENU_ARROW_WIDTH;
    }
    return {qMax(width, MENU_MIN_WIDTH), style::MENU_ITEM_HEIGHT};
}

/// The geometry of an arrowless scroll bar: the groove is the whole bar and the slider's length
/// shows the visible share of the content.
QRect scrollBarRect(const QStyleOptionSlider &bar, QStyle::SubControl subControl)
{
    const QRect groove = bar.rect;
    const bool horizontal = bar.orientation == Qt::Horizontal;
    const int length = horizontal ? groove.width() : groove.height();
    const qint64 range = qint64(bar.maximum) - bar.minimum;
    int sliderLength = length;
    if (range > 0) {
        sliderLength = static_cast<int>(qint64(length) * bar.pageStep / (range + bar.pageStep));
        sliderLength = qBound(qMin(SCROLLBAR_MIN_SLIDER, length), sliderLength, length);
    }
    const int start = QStyle::sliderPositionFromValue(bar.minimum, bar.maximum, bar.sliderPosition, length - sliderLength, bar.upsideDown);
    const QRect slider = horizontal ? QRect(groove.left() + start, groove.top(), sliderLength, groove.height())
                                    : QRect(groove.left(), groove.top() + start, groove.width(), sliderLength);
    switch (subControl) {
    case QStyle::SC_ScrollBarGroove:
        return groove;
    case QStyle::SC_ScrollBarSlider:
        return slider;
    case QStyle::SC_ScrollBarSubPage:
        return horizontal ? QRect(groove.left(), groove.top(), start, groove.height())
                          : QRect(groove.left(), groove.top(), groove.width(), start);
    case QStyle::SC_ScrollBarAddPage:
        return horizontal ? QRect(slider.right() + 1, groove.top(), groove.right() - slider.right(), groove.height())
                          : QRect(groove.left(), slider.bottom() + 1, groove.width(), groove.bottom() - slider.bottom());
    default:
        return {};
    }
}

} // namespace

FinderStyle::FinderStyle(QStyle *base)
    : QProxyStyle(base)
{
}

void FinderStyle::polish(QWidget *widget)
{
    if (isGlassPopup(widget)) {
        widget->setProperty(SKIP_SHADOW_PROPERTY, true);
    }
    QProxyStyle::polish(widget);
    if (isGlassPopup(widget)) {
        widget->setAttribute(Qt::WA_TranslucentBackground);
        if (qobject_cast<QMenu *>(widget)) {
            widget->setFont(style::uiFont(13));
        }
        widget->removeEventFilter(this);
        widget->installEventFilter(this);
    } else if (qobject_cast<QScrollBar *>(widget)) {
        widget->setAttribute(Qt::WA_Hover);
    }
    // Every button and popup button shows the hand, as everything clickable in this theme does; a widget that sets
    // its own cursor keeps it, and a text field keeps its I-beam.
    const auto *combo = qobject_cast<QComboBox *>(widget);
    if ((qobject_cast<QAbstractButton *>(widget) || (combo && !combo->isEditable())) && !widget->testAttribute(Qt::WA_SetCursor)) {
        widget->setCursor(Qt::PointingHandCursor);
    }
}

void FinderStyle::unpolish(QWidget *widget)
{
    widget->removeEventFilter(this);
    QProxyStyle::unpolish(widget);
}

bool FinderStyle::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Show || event->type() == QEvent::Resize) {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget->isWindow() && widget->windowHandle()) {
            blurPopup(widget);
            // Again once the event is done: Breeze's blur helper, when the user lowered the menu
            // opacity, handles the same event after this filter and sets its own, squarer region.
            QTimer::singleShot(0, widget, [widget] { blurPopup(widget); });
            if (event->type() == QEvent::Show) {
                attachShadow(widget, {popupRadius(widget), 28, 8, 9, 115});
            }
        }
    }
    return QProxyStyle::eventFilter(watched, event);
}

void FinderStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    switch (element) {
    case PE_PanelMenu:
        style::paintGlass(*painter, QRectF(option->rect), style::MENU_RADIUS, style::MENU_FILL);
        return;
    case PE_FrameMenu:
    case PE_FrameFocusRect:
        return;
    case PE_PanelTipLabel:
        style::paintGlass(*painter, QRectF(option->rect), TOOLTIP_RADIUS, TOOLTIP_FILL);
        return;
    case PE_PanelItemViewItem: {
        const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option);
        const bool selected = option->state & State_Selected;
        const bool hovered = option->state & State_MouseOver;
        if (!item || (!selected && !hovered)) {
            return;
        }
        // An icon tile gets the translucent box macOS puts behind a selected icon; a list row gets
        // the solid accent bar of Finder's list view.
        const bool tile = item->rect.height() > TILE_MIN_HEIGHT;
        const bool active = option->state & State_Active;
        QColor fill = style::HOVER;
        if (selected && tile) {
            fill = active ? style::SELECTION_TILE : style::SELECTION_TILE_INACTIVE;
        } else if (selected) {
            fill = active ? style::SELECTION : style::SELECTION_INACTIVE;
        }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF rect = QRectF(item->rect).adjusted(0.5, 0.5, -0.5, -0.5);
        const bool alone = item->viewItemPosition == QStyleOptionViewItem::OnlyOne
            || item->viewItemPosition == QStyleOptionViewItem::Invalid;
        const qreal radius = tile ? 8.0 : alone ? 6.0 : 0.0;
        QPainterPath path;
        path.addRoundedRect(rect, radius, radius);
        painter->fillPath(path, fill);
        painter->restore();
        return;
    }
    case PE_IndicatorBranch:
        // Finder's lists show a disclosure chevron on folders and no tree lines (Dolphin asks for the
        // lines with State_Sibling and State_Item, kstandarditemlistwidget.cpp, v26.08.2, line 1701).
        if (option->state & State_Children) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            const Qt::ArrowType direction = (option->state & State_Open) ? Qt::DownArrow : Qt::RightArrow;
            drawChevron(painter, QRectF(option->rect).center(), 8.0, direction, style::TEXT_SECONDARY);
            painter->restore();
        }
        return;
    case PE_IndicatorHeaderArrow: {
        const auto *header = qstyleoption_cast<const QStyleOptionHeader *>(option);
        if (!header || header->sortIndicator == QStyleOptionHeader::None) {
            return;
        }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const Qt::ArrowType direction = header->sortIndicator == QStyleOptionHeader::SortUp ? Qt::UpArrow : Qt::DownArrow;
        drawChevron(painter, QRectF(option->rect).center(), 8.0, direction, style::TEXT_SECONDARY);
        painter->restore();
        return;
    }
    default:
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
}

void FinderStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    switch (element) {
    case CE_MenuItem:
        if (const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            drawMenuItem(*item, painter);
            painter->restore();
            return;
        }
        break;
    case CE_MenuEmptyArea:
        return;
    case CE_Splitter:
        // The split view's divider: a hairline of the glass rim's white, not Breeze's grip.
        painter->fillRect(option->rect, style::GLASS_RIM);
        return;
    case CE_FocusFrame:
        // Breeze 6.8 puts a QFocusFrame around a line edit that Tab or a shortcut focused (Style::event) and draws it
        // here; the toolbar's fields draw their own capsule ring (style::paintFocusRing), so Breeze's rounded
        // rectangle would be a second, misshapen ring around them.
        return;
    case CE_HeaderSection:
    case CE_HeaderEmptyArea: {
        painter->save();
        painter->setPen(QPen(style::HAIRLINE, 1.0));
        const QRectF rect(option->rect);
        painter->drawLine(QPointF(rect.left(), rect.bottom() - 0.5), QPointF(rect.right() + 1, rect.bottom() - 0.5));
        const auto *header = qstyleoption_cast<const QStyleOptionHeader *>(option);
        if (element == CE_HeaderSection && header && header->position != QStyleOptionHeader::End
            && header->position != QStyleOptionHeader::OnlyOneSection) {
            painter->drawLine(QPointF(rect.right() + 0.5, rect.top() + 6), QPointF(rect.right() + 0.5, rect.bottom() - 6));
        }
        painter->restore();
        return;
    }
    case CE_HeaderLabel:
        if (const auto *header = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            painter->save();
            painter->setFont(style::uiFont(11, QFont::DemiBold));
            painter->setPen(style::TEXT_SECONDARY);
            const QRect rect = header->rect.adjusted(4, 0, -4, 0);
            const QString text = painter->fontMetrics().elidedText(header->text, Qt::ElideRight, rect.width());
            painter->drawText(rect, Qt::AlignVCenter | header->textAlignment, text);
            painter->restore();
            return;
        }
        break;
    default:
        break;
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void FinderStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    const auto *bar = qstyleoption_cast<const QStyleOptionSlider *>(option);
    if (control != CC_ScrollBar || !bar) {
        QProxyStyle::drawComplexControl(control, option, painter, widget);
        return;
    }
    if (bar->maximum <= bar->minimum) {
        return;
    }
    const bool hovered = option->state & State_MouseOver;
    const bool pressed = (option->state & State_Sunken) && (bar->activeSubControls & SC_ScrollBarSlider);
    const qreal thickness = hovered || pressed ? 8.0 : 6.0;
    QRectF slider = scrollBarRect(*bar, SC_ScrollBarSlider);
    if (bar->orientation == Qt::Horizontal) {
        slider = QRectF(slider.left() + 2, option->rect.bottom() - 2 - thickness, slider.width() - 4, thickness);
    } else {
        slider = QRectF(option->rect.right() - 2 - thickness, slider.top() + 2, thickness, slider.height() - 4);
    }
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(hovered || pressed ? style::SCROLLER_HOVER : style::SCROLLER);
    painter->drawRoundedRect(slider, thickness / 2.0, thickness / 2.0);
    painter->restore();
}

QRect FinderStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    if (control == CC_ScrollBar) {
        if (const auto *bar = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            return scrollBarRect(*bar, subControl);
        }
    }
    return QProxyStyle::subControlRect(control, option, subControl, widget);
}

QStyle::SubControl FinderStyle::hitTestComplexControl(ComplexControl control, const QStyleOptionComplex *option, const QPoint &pos,
                                                      const QWidget *widget) const
{
    if (control == CC_ScrollBar) {
        if (const auto *bar = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            for (const SubControl part : {SC_ScrollBarSlider, SC_ScrollBarSubPage, SC_ScrollBarAddPage}) {
                if (scrollBarRect(*bar, part).contains(pos)) {
                    return part;
                }
            }
            return SC_None;
        }
    }
    return QProxyStyle::hitTestComplexControl(control, option, pos, widget);
}

int FinderStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    switch (metric) {
    case PM_MenuHMargin:
    case PM_MenuPanelWidth:
        return 0;
    case PM_MenuVMargin:
        return 5;
    case PM_ScrollBarExtent:
        return SCROLLBAR_EXTENT;
    case PM_ScrollBarSliderMin:
        return SCROLLBAR_MIN_SLIDER;
    default:
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

QSize FinderStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const
{
    if (type == CT_MenuItem) {
        if (const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            return menuItemSize(*item, size);
        }
    }
    return QProxyStyle::sizeFromContents(type, option, size, widget);
}

int FinderStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    // The file view's wheel scrolling glides over this time (Dolphin's KItemListSmoothScroller reads it); Breeze's
    // 100 ms reads as a jump. It follows the desktop's animation speed like every other transition.
    if (hint == SH_Widget_Animation_Duration) {
        return motion::scaled(style::SCROLL_GLIDE_MS, motion::speed());
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

} // namespace fe
