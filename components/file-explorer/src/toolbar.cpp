// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements toolbar.h.

#include "toolbar.h"

#include "addressfield.h"
#include "style.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>

namespace fe {

namespace {

constexpr int SEARCH_MIN_WIDTH = 150;
/// The address field never gets narrower than this; below it the buttons fold away first.
constexpr int ADDRESS_MIN_WIDTH = 200;
/// The inner padding and gaps of a text button, and the size of its chevron.
constexpr int LABEL_PADDING = 12;
constexpr int LABEL_GAP = 6;
constexpr int CHEVRON_SIZE = 12;

} // namespace

GlassButton::GlassButton(QString iconName, const QString &toolTip, QWidget *parent)
    : QAbstractButton(parent)
    , m_iconName(std::move(iconName))
    , m_hover(new motion::HoverFade(style::HOVER_FADE_MS, this))
{
    connect(m_hover, &motion::HoverFade::changed, this, qOverload<>(&QWidget::update));
    setToolTip(toolTip);
    // Set on the button itself, so the window edge's resize cursors never reach it.
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(sizeHint());
}

void GlassButton::setStandalone(bool standalone)
{
    m_standalone = standalone;
    setFixedSize(sizeHint());
    update();
}

void GlassButton::setIconName(const QString &iconName)
{
    m_iconName = iconName;
    update();
}

void GlassButton::setLabel(const QString &label)
{
    m_label = label;
    setFont(style::uiFont(13));
    setFixedSize(sizeHint());
    update();
}

void GlassButton::setChevron(bool chevron)
{
    m_chevron = chevron;
    setFixedSize(sizeHint());
    update();
}

QSize GlassButton::sizeHint() const
{
    if (m_label.isEmpty()) {
        return {m_standalone ? style::CAPSULE_HEIGHT : style::BUTTON_WIDTH, style::CAPSULE_HEIGHT};
    }
    int width = 2 * LABEL_PADDING + fontMetrics().horizontalAdvance(m_label);
    if (!m_iconName.isEmpty()) {
        width += style::TOOL_ICON_SIZE + LABEL_GAP;
    }
    if (m_chevron) {
        width += CHEVRON_SIZE + 2;
    }
    return {width, style::CAPSULE_HEIGHT};
}

void GlassButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds(rect());
    if (m_standalone) {
        style::paintGlass(painter, bounds, bounds.height() / 2.0, style::CAPSULE_FILL);
    }

    QColor state;
    if (isDown()) {
        state = style::CAPSULE_PRESSED;
    } else if (isChecked()) {
        state = style::SEGMENT_CHECKED;
    } else if (m_hover->progress() > 0.0 && isEnabled()) {
        state = style::CAPSULE_HOVER;
        state.setAlphaF(state.alphaF() * m_hover->progress());
    }
    if (state.isValid()) {
        const QRectF inner = m_standalone ? bounds : bounds.adjusted(2, 3, -2, -3);
        QPainterPath path;
        path.addRoundedRect(inner, inner.height() / 2.0, inner.height() / 2.0);
        painter.fillPath(path, state);
    }

    const QColor iconColor = isEnabled() ? style::TEXT : style::TEXT_TERTIARY;
    if (m_label.isEmpty()) {
        const QPixmap icon = style::tintedIcon(m_iconName, style::TOOL_ICON_SIZE, iconColor, devicePixelRatioF());
        const QPointF topLeft((width() - style::TOOL_ICON_SIZE) / 2.0, (height() - style::TOOL_ICON_SIZE) / 2.0);
        painter.drawPixmap(topLeft, icon);
        return;
    }
    qreal x = LABEL_PADDING;
    if (!m_iconName.isEmpty()) {
        const QPixmap icon = style::tintedIcon(m_iconName, style::TOOL_ICON_SIZE, iconColor, devicePixelRatioF());
        painter.drawPixmap(QPointF(x, (height() - style::TOOL_ICON_SIZE) / 2.0), icon);
        x += style::TOOL_ICON_SIZE + LABEL_GAP;
    }
    painter.setPen(iconColor);
    const int textWidth = fontMetrics().horizontalAdvance(m_label);
    painter.drawText(QRectF(x, 0, textWidth + 1, height()), Qt::AlignLeft | Qt::AlignVCenter, m_label);
    if (m_chevron) {
        const QPixmap chevron = style::tintedIcon(QStringLiteral(":/icons/actions/chevron-down.svg"), CHEVRON_SIZE, iconColor,
                                                  devicePixelRatioF());
        painter.drawPixmap(QPointF(x + textWidth + 2, (height() - CHEVRON_SIZE) / 2.0 + 1), chevron);
    }
}

void GlassButton::enterEvent(QEnterEvent *event)
{
    m_hover->setHovered(true);
    QAbstractButton::enterEvent(event);
}

void GlassButton::leaveEvent(QEvent *event)
{
    m_hover->setHovered(false);
    QAbstractButton::leaveEvent(event);
}

Capsule::Capsule(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
{
    m_layout->setContentsMargins(3, 0, 3, 0);
    m_layout->setSpacing(0);
    setFixedHeight(style::CAPSULE_HEIGHT);
}

void Capsule::addButton(GlassButton *button)
{
    m_layout->addWidget(button);
}

void Capsule::addDivider()
{
    m_dividers.append(m_layout->count());
}

void Capsule::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QRectF bounds(rect());
    style::paintGlass(painter, bounds, bounds.height() / 2.0, style::CAPSULE_FILL);
    painter.setPen(QPen(style::HAIRLINE, 1.0));
    for (const int before : std::as_const(m_dividers)) {
        if (before <= 0 || before >= m_layout->count()) {
            continue;
        }
        const QWidget *widget = m_layout->itemAt(before)->widget();
        const qreal x = widget->geometry().left() + 0.5;
        painter.drawLine(QPointF(x, 10), QPointF(x, height() - 10));
    }
}

SearchField::SearchField(QWidget *parent)
    : QLineEdit(parent)
{
    setPlaceholderText(QStringLiteral("Search"));
    setFrame(false);
    setFont(style::uiFont(13));
    setFixedSize(style::SEARCH_WIDTH, style::CAPSULE_HEIGHT);
    setTextMargins(30, 0, 12, 0);
    setClearButtonEnabled(true);
    QPalette searchPalette = palette();
    searchPalette.setColor(QPalette::Base, Qt::transparent);
    searchPalette.setColor(QPalette::Text, style::TEXT);
    searchPalette.setColor(QPalette::PlaceholderText, style::TEXT_SECONDARY);
    setPalette(searchPalette);
    setAttribute(Qt::WA_MacShowFocusRect, false);
}

void SearchField::paintEvent(QPaintEvent *event)
{
    {
        QPainter painter(this);
        const QRectF bounds(rect());
        style::paintGlass(painter, bounds, bounds.height() / 2.0, hasFocus() ? style::CAPSULE_HOVER : style::CAPSULE_FILL);
        if (hasFocus()) {
            style::paintFocusRing(painter, bounds.adjusted(1, 1, -1, -1));
        }
        const int size = 15;
        const QPixmap glass = style::tintedIcon(QStringLiteral("system-search-symbolic"), size, style::TEXT_SECONDARY, devicePixelRatioF());
        painter.drawPixmap(QPointF(11, (height() - size) / 2.0), glass);
    }
    QLineEdit::paintEvent(event);
}

Toolbar::Toolbar(const QString &homePath, QWidget *parent)
    : QWidget(parent)
    , m_leading(new QHBoxLayout)
    , m_trailing(new QHBoxLayout)
    , m_back(new GlassButton(QStringLiteral("go-previous-symbolic"), QStringLiteral("Back")))
    , m_forward(new GlassButton(QStringLiteral("go-next-symbolic"), QStringLiteral("Forward")))
    , m_up(new GlassButton(QStringLiteral("go-up-symbolic"), QStringLiteral("Enclosing Folder"), this))
    , m_address(new AddressField(homePath, this))
    , m_viewModes(new QButtonGroup(this))
    , m_group(new GlassButton(QString(), QStringLiteral("Group files by name, kind, date or size"), this))
    , m_share(new GlassButton(QStringLiteral(":/icons/actions/share.svg"), QStringLiteral("Share"), this))
    , m_tags(new GlassButton(QStringLiteral(":/icons/actions/tag.svg"), QStringLiteral("Tag the selected items with a color"), this))
    , m_more(new GlassButton(QStringLiteral(":/icons/actions/more.svg"), QStringLiteral("More"), this))
    , m_search(new SearchField(this))
{
    setFixedHeight(style::TOOLBAR_HEIGHT);

    auto *navigation = new Capsule(this);
    navigation->addButton(m_back);
    navigation->addButton(m_forward);
    connect(m_back, &QAbstractButton::clicked, this, &Toolbar::backClicked);
    connect(m_forward, &QAbstractButton::clicked, this, &Toolbar::forwardClicked);
    m_up->setStandalone(true);
    connect(m_up, &QAbstractButton::clicked, this, &Toolbar::upClicked);
    m_address->setMinimumWidth(ADDRESS_MIN_WIDTH);
    m_address->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *views = new Capsule(this);
    const struct {
        const char *icon;
        const char *tip;
        ViewMode mode;
    } modes[] = {
        {"view-grid-symbolic", "as Icons", ViewMode::Icons},
        {"view-list-symbolic", "as List", ViewMode::List},
        {"view-list-compact-symbolic", "as Compact List", ViewMode::Compact},
        {"view-file-columns-symbolic", "as Columns", ViewMode::Columns},
        {"view-preview-symbolic", "as Gallery", ViewMode::Gallery},
    };
    for (const auto &mode : modes) {
        auto *button = new GlassButton(QString::fromLatin1(mode.icon), QString::fromLatin1(mode.tip));
        button->setCheckable(true);
        m_viewModes->addButton(button, static_cast<int>(mode.mode));
        views->addButton(button);
    }
    m_viewModes->setExclusive(true);
    connect(m_viewModes, &QButtonGroup::idClicked, this, [this](int id) { Q_EMIT viewModeChosen(static_cast<ViewMode>(id)); });

    m_group->setStandalone(true);
    m_group->setLabel(QStringLiteral("Group"));
    m_group->setChevron(true);
    m_share->setStandalone(true);
    m_tags->setStandalone(true);
    m_tags->setLabel(QStringLiteral("Edit Tags"));
    m_more->setStandalone(true);
    connect(m_group, &QAbstractButton::clicked, this, &Toolbar::groupClicked);
    connect(m_share, &QAbstractButton::clicked, this, &Toolbar::shareClicked);
    connect(m_tags, &QAbstractButton::clicked, this, &Toolbar::editTagsClicked);
    connect(m_more, &QAbstractButton::clicked, this, &Toolbar::moreClicked);
    connect(m_search, &QLineEdit::textChanged, this, &Toolbar::searchChanged);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 12, 0);
    layout->setSpacing(8);
    m_leading->setContentsMargins(0, 0, 0, 0);
    m_leading->setSpacing(0);
    layout->addLayout(m_leading);
    layout->addWidget(navigation);
    layout->addWidget(m_up);
    layout->addWidget(m_address, 1);
    layout->addWidget(views);
    layout->addWidget(m_group);
    layout->addWidget(m_share);
    layout->addWidget(m_tags);
    layout->addWidget(m_more);
    layout->addWidget(m_search);
    m_trailing->setContentsMargins(0, 0, 0, 0);
    m_trailing->setSpacing(0);
    layout->addSpacing(4);
    layout->addLayout(m_trailing);

    // Everything they do stays in the More menu, the context menu and the menu bar.
    m_collapsible = {m_tags, m_share, m_group, views};
}

void Toolbar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    fitToWidth();
}

void Toolbar::fitToWidth()
{
    QLayout *bar = layout();
    // The size hint counts the address field at its minimum width and skips hidden buttons.
    const auto overflow = [this, bar] {
        bar->invalidate();
        return bar->sizeHint().width() - width();
    };
    m_search->setFixedWidth(style::SEARCH_WIDTH);
    for (QWidget *button : std::as_const(m_collapsible)) {
        button->setVisible(true);
    }
    m_tags->setLabel(QStringLiteral("Edit Tags"));
    if (const int excess = overflow(); excess > 0) {
        m_search->setFixedWidth(qMax(SEARCH_MIN_WIDTH, style::SEARCH_WIDTH - excess));
    }
    // Edit Tags keeps its icon a while after its words go.
    if (overflow() > 0) {
        m_tags->setLabel(QString());
    }
    for (QWidget *button : std::as_const(m_collapsible)) {
        if (overflow() <= 0) {
            break;
        }
        button->setVisible(false);
    }
}

void Toolbar::setNavigationState(bool canGoBack, bool canGoForward, bool canGoUp)
{
    m_back->setEnabled(canGoBack);
    m_forward->setEnabled(canGoForward);
    m_up->setEnabled(canGoUp);
}

void Toolbar::setViewMode(ViewMode mode)
{
    if (QAbstractButton *button = m_viewModes->button(static_cast<int>(mode))) {
        button->setChecked(true);
    }
}

void Toolbar::setSearchText(const QString &term)
{
    if (m_search->text() == term) {
        return;
    }
    const QSignalBlocker blocker(m_search);
    m_search->setText(term);
}

QHBoxLayout *Toolbar::leadingLayout() const
{
    return m_leading;
}

QHBoxLayout *Toolbar::trailingLayout() const
{
    return m_trailing;
}

SearchField *Toolbar::searchField() const
{
    return m_search;
}

AddressField *Toolbar::addressField() const
{
    return m_address;
}

GlassButton *Toolbar::groupButton() const
{
    return m_group;
}

GlassButton *Toolbar::shareButton() const
{
    return m_share;
}

GlassButton *Toolbar::tagsButton() const
{
    return m_tags;
}

GlassButton *Toolbar::moreButton() const
{
    return m_more;
}

} // namespace fe
