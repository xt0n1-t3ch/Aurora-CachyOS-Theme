// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements toolbar.h.

#include "toolbar.h"

#include "style.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>

namespace fe {

namespace {

constexpr int SEARCH_MIN_WIDTH = 150;
constexpr int TITLE_MIN_WIDTH = 60;

} // namespace

/// The folder name in the toolbar: bold, on one line, and elided only once nothing else can give way.
class TitleLabel : public QWidget
{
public:
    explicit TitleLabel(QWidget *parent)
        : QWidget(parent)
    {
        setFont(style::uiFont(15, QFont::Bold));
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }

    void setText(const QString &text)
    {
        if (text != m_text) {
            m_text = text;
            updateGeometry();
            update();
        }
    }

    QSize sizeHint() const override
    {
        return {fontMetrics().horizontalAdvance(m_text) + 2, fontMetrics().height()};
    }

    QSize minimumSizeHint() const override
    {
        const QSize full = sizeHint();
        return {qMin(full.width(), TITLE_MIN_WIDTH), full.height()};
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setPen(style::TEXT);
        painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(m_text, Qt::ElideRight, width()));
    }

private:
    QString m_text;
};

GlassButton::GlassButton(QString iconName, const QString &toolTip, QWidget *parent)
    : QAbstractButton(parent)
    , m_iconName(std::move(iconName))
{
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

QSize GlassButton::sizeHint() const
{
    return {m_standalone ? style::CAPSULE_HEIGHT : style::BUTTON_WIDTH, style::CAPSULE_HEIGHT};
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
    } else if (m_hover && isEnabled()) {
        state = style::CAPSULE_HOVER;
    }
    if (state.isValid()) {
        const QRectF inner = m_standalone ? bounds : bounds.adjusted(2, 3, -2, -3);
        QPainterPath path;
        path.addRoundedRect(inner, inner.height() / 2.0, inner.height() / 2.0);
        painter.fillPath(path, state);
    }

    const QColor iconColor = isEnabled() ? style::TEXT : style::TEXT_TERTIARY;
    const QPixmap icon = style::tintedIcon(m_iconName, style::TOOL_ICON_SIZE, iconColor, devicePixelRatioF());
    const QPointF topLeft((width() - style::TOOL_ICON_SIZE) / 2.0, (height() - style::TOOL_ICON_SIZE) / 2.0);
    painter.drawPixmap(topLeft, icon);
}

void GlassButton::enterEvent(QEnterEvent *event)
{
    m_hover = true;
    update();
    QAbstractButton::enterEvent(event);
}

void GlassButton::leaveEvent(QEvent *event)
{
    m_hover = false;
    update();
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
        const int size = 15;
        const QPixmap glass = style::tintedIcon(QStringLiteral("system-search-symbolic"), size, style::TEXT_SECONDARY, devicePixelRatioF());
        painter.drawPixmap(QPointF(11, (height() - size) / 2.0), glass);
    }
    QLineEdit::paintEvent(event);
}

Toolbar::Toolbar(QWidget *parent)
    : QWidget(parent)
    , m_leading(new QHBoxLayout)
    , m_trailing(new QHBoxLayout)
    , m_back(new GlassButton(QStringLiteral("go-previous-symbolic"), QStringLiteral("Back")))
    , m_forward(new GlassButton(QStringLiteral("go-next-symbolic"), QStringLiteral("Forward")))
    , m_up(new GlassButton(QStringLiteral("go-up-symbolic"), QStringLiteral("Enclosing Folder")))
    , m_title(new TitleLabel(this))
    , m_viewModes(new QButtonGroup(this))
    , m_sort(new GlassButton(QStringLiteral(":/icons/sort.svg"), QStringLiteral("Sort By")))
    , m_search(new SearchField(this))
{
    setFixedHeight(style::TOOLBAR_HEIGHT);

    auto *navigation = new Capsule(this);
    navigation->addButton(m_back);
    navigation->addButton(m_forward);
    navigation->addButton(m_up);
    connect(m_back, &QAbstractButton::clicked, this, &Toolbar::backClicked);
    connect(m_forward, &QAbstractButton::clicked, this, &Toolbar::forwardClicked);
    connect(m_up, &QAbstractButton::clicked, this, &Toolbar::upClicked);

    auto *newFolder = new GlassButton(QStringLiteral("folder-new-symbolic"), QStringLiteral("New Folder"), this);
    newFolder->setStandalone(true);
    connect(newFolder, &QAbstractButton::clicked, this, &Toolbar::newFolderClicked);

    auto *views = new Capsule(this);
    const struct {
        const char *icon;
        const char *tip;
        ViewMode mode;
    } modes[] = {
        {"view-grid-symbolic", "as Icons", ViewMode::Icons},
        {"view-list-symbolic", "as List", ViewMode::List},
        {"view-list-compact-symbolic", "as Compact List", ViewMode::Compact},
    };
    for (const auto &mode : modes) {
        auto *button = new GlassButton(QString::fromLatin1(mode.icon), QString::fromLatin1(mode.tip));
        button->setCheckable(true);
        m_viewModes->addButton(button, static_cast<int>(mode.mode));
        views->addButton(button);
    }
    m_viewModes->setExclusive(true);
    connect(m_viewModes, &QButtonGroup::idClicked, this, [this](int id) { Q_EMIT viewModeChosen(static_cast<ViewMode>(id)); });

    auto *sortCapsule = new Capsule(this);
    sortCapsule->addButton(m_sort);
    connect(m_sort, &QAbstractButton::clicked, this, &Toolbar::sortClicked);

    m_addToSidebar = new GlassButton(QStringLiteral(":/icons/bookmark.svg"), QStringLiteral("Add to Sidebar"), this);
    m_addToSidebar->setStandalone(true);
    connect(m_addToSidebar, &QAbstractButton::clicked, this, &Toolbar::addToSidebarClicked);

    connect(m_search, &QLineEdit::textChanged, this, &Toolbar::searchChanged);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 12, 0);
    layout->setSpacing(8);
    m_leading->setContentsMargins(0, 0, 0, 0);
    m_leading->setSpacing(0);
    layout->addLayout(m_leading);
    layout->addWidget(navigation);
    layout->addSpacing(6);
    layout->addWidget(m_title);
    layout->addStretch(1);
    layout->addWidget(newFolder);
    layout->addWidget(views);
    layout->addWidget(sortCapsule);
    layout->addWidget(m_addToSidebar);
    layout->addWidget(m_search);
    m_trailing->setContentsMargins(0, 0, 0, 0);
    m_trailing->setSpacing(0);
    layout->addSpacing(4);
    layout->addLayout(m_trailing);

    m_collapsible = {m_addToSidebar, newFolder, sortCapsule, views};
}

void Toolbar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    fitToWidth();
}

void Toolbar::fitToWidth()
{
    QLayout *bar = layout();
    // The size hint counts the title at its full width and skips hidden buttons.
    const auto overflow = [this, bar] {
        bar->invalidate();
        return bar->sizeHint().width() - width();
    };
    m_search->setFixedWidth(style::SEARCH_WIDTH);
    for (QWidget *button : std::as_const(m_collapsible)) {
        button->setVisible(true);
    }
    if (const int excess = overflow(); excess > 0) {
        m_search->setFixedWidth(qMax(SEARCH_MIN_WIDTH, style::SEARCH_WIDTH - excess));
    }
    for (QWidget *button : std::as_const(m_collapsible)) {
        if (overflow() <= 0) {
            break;
        }
        button->setVisible(false);
    }
}

void Toolbar::setTitle(const QString &title)
{
    m_title->setText(title);
}

void Toolbar::setAddToSidebarEnabled(bool enabled)
{
    m_addToSidebar->setEnabled(enabled);
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

GlassButton *Toolbar::sortButton() const
{
    return m_sort;
}

} // namespace fe
