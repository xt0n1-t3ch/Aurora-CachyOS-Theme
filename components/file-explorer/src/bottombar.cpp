// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements bottombar.h.

#include "bottombar.h"

#include "style.h"

#include <QAbstractButton>
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>

namespace fe {

namespace {

constexpr int STATUS_ICON_SIZE = 16;
constexpr int CRUMB_ICON_SIZE = 14;
constexpr qreal CRUMB_ICON_GAP = 5.0;
constexpr qreal CHEVRON_WIDTH = 16.0;
constexpr qreal PATH_LEFT = 14.0;

/// A borderless icon button of the status row; the accent color marks it as on.
class FlatButton : public QAbstractButton
{
public:
    FlatButton(QString iconName, const QString &toolTip, QWidget *parent)
        : QAbstractButton(parent)
        , m_iconName(std::move(iconName))
    {
        setToolTip(toolTip);
        setFocusPolicy(Qt::NoFocus);
        setFixedSize(28, 24);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        QColor color = style::TEXT_SECONDARY;
        if (isChecked()) {
            color = style::ACCENT;
        } else if (m_hover) {
            color = style::TEXT;
        }
        const QPixmap icon = style::tintedIcon(m_iconName, STATUS_ICON_SIZE, color, devicePixelRatioF());
        painter.drawPixmap(QPointF((width() - STATUS_ICON_SIZE) / 2.0, (height() - STATUS_ICON_SIZE) / 2.0), icon);
    }

    void enterEvent(QEnterEvent *event) override
    {
        m_hover = true;
        update();
        QAbstractButton::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        m_hover = false;
        update();
        QAbstractButton::leaveEvent(event);
    }

private:
    QString m_iconName;
    bool m_hover = false;
};

/// The colored icon in front of a path segment: home, the disk, a special place or a folder.
QString crumbIcon(const Crumb &crumb, bool first)
{
    const QString scheme = crumb.url.scheme();
    if (scheme == u"trash") {
        return QStringLiteral("user-trash");
    }
    if (scheme == u"recentlyused") {
        return QStringLiteral("document-open-recent");
    }
    if (!crumb.url.isLocalFile()) {
        return QStringLiteral("folder-network");
    }
    if (first) {
        return crumb.url.path() == u"/" ? QStringLiteral("drive-harddisk") : QStringLiteral("user-home");
    }
    return QStringLiteral("folder");
}

} // namespace

StatusRow::StatusRow(QWidget *parent)
    : QWidget(parent)
    , m_hidden(new FlatButton(QStringLiteral("view-hidden-symbolic"), QStringLiteral("Show Hidden Files"), this))
    , m_previews(new FlatButton(QStringLiteral("view-preview-symbolic"), QStringLiteral("Show Previews"), this))
{
    setFixedHeight(style::STATUS_HEIGHT);
    m_hidden->setCheckable(true);
    m_previews->setCheckable(true);
    auto *info = new FlatButton(QStringLiteral("dialog-information-symbolic"), QStringLiteral("Get Info"), this);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 0, 14, 0);
    layout->setSpacing(2);
    layout->addWidget(m_hidden);
    layout->addWidget(m_previews);
    layout->addWidget(info);
    layout->addStretch();

    // The buttons only ask; the window flips the setting and reports the new state back.
    connect(m_hidden, &QAbstractButton::clicked, this, &StatusRow::hiddenToggled);
    connect(m_previews, &QAbstractButton::clicked, this, &StatusRow::previewsToggled);
    connect(info, &QAbstractButton::clicked, this, &StatusRow::infoClicked);
}

void StatusRow::setText(const QString &text)
{
    if (text != m_text) {
        m_text = text;
        update();
    }
}

void StatusRow::setHiddenShown(bool shown)
{
    m_hidden->setChecked(shown);
    m_hidden->setToolTip(shown ? QStringLiteral("Hide Hidden Files") : QStringLiteral("Show Hidden Files"));
}

void StatusRow::setPreviewsShown(bool shown)
{
    m_previews->setChecked(shown);
    m_previews->setToolTip(shown ? QStringLiteral("Hide Previews") : QStringLiteral("Show Previews"));
}

void StatusRow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setFont(style::uiFont(12));
    painter.setPen(style::TEXT_SECONDARY);
    const QRect textRect = rect().adjusted(110, 0, -14, 0);
    painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(m_text, Qt::ElideMiddle, textRect.width()));
}

PathBar::PathBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(style::PATHBAR_HEIGHT);
    setMouseTracking(true);
    setFont(style::uiFont(12));
}

void PathBar::setCrumbs(const QList<Crumb> &crumbs)
{
    m_crumbs = crumbs;
    m_hover = -1;
    layoutSegments();
    update();
}

void PathBar::layoutSegments()
{
    m_segments.clear();
    for (qsizetype i = 0; i < m_crumbs.size(); ++i) {
        m_segments.append({m_crumbs.at(i), crumbIcon(m_crumbs.at(i), i == 0), {}, false});
    }

    const QFontMetricsF metrics(font());
    const auto segmentWidth = [&metrics](const Segment &segment) {
        if (segment.ellipsis) {
            return metrics.horizontalAdvance(QStringLiteral("…"));
        }
        return CRUMB_ICON_SIZE + CRUMB_ICON_GAP + metrics.horizontalAdvance(segment.crumb.label);
    };
    const auto totalWidth = [&] {
        qreal total = PATH_LEFT;
        for (const Segment &segment : std::as_const(m_segments)) {
            total += segmentWidth(segment) + CHEVRON_WIDTH;
        }
        return total;
    };
    // Too long for the window: fold the segments between the first and the last into one "…", the outermost first.
    while (totalWidth() > width() - PATH_LEFT && m_segments.size() > 3) {
        if (m_segments.at(1).ellipsis) {
            m_segments.removeAt(2);
        } else {
            m_segments[1].ellipsis = true;
        }
    }

    qreal x = PATH_LEFT;
    for (Segment &segment : m_segments) {
        const qreal segmentW = segmentWidth(segment);
        segment.rect = QRectF(x, 0, segmentW, height());
        x += segmentW + CHEVRON_WIDTH;
    }
}

int PathBar::segmentAt(QPointF pos) const
{
    for (qsizetype i = 0; i < m_segments.size(); ++i) {
        if (!m_segments.at(i).ellipsis && m_segments.at(i).rect.adjusted(-3, 0, 3, 0).contains(pos)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void PathBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font());
    const qreal dpr = devicePixelRatioF();
    const qreal middle = height() / 2.0;

    for (qsizetype i = 0; i < m_segments.size(); ++i) {
        const Segment &segment = m_segments.at(i);
        const bool last = i == m_segments.size() - 1;
        const QColor textColor = (last || m_hover == i) ? style::TEXT : style::TEXT_SECONDARY;
        if (segment.ellipsis) {
            painter.setPen(style::TEXT_SECONDARY);
            painter.drawText(segment.rect, Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("…"));
        } else {
            const QPixmap icon = QIcon::fromTheme(segment.iconName, QIcon::fromTheme(QStringLiteral("folder")))
                                     .pixmap(QSize(CRUMB_ICON_SIZE, CRUMB_ICON_SIZE), dpr);
            painter.drawPixmap(QPointF(segment.rect.left(), middle - CRUMB_ICON_SIZE / 2.0), icon);
            painter.setPen(textColor);
            const QRectF textRect = segment.rect.adjusted(CRUMB_ICON_SIZE + CRUMB_ICON_GAP, 0, 0, 0);
            painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, segment.crumb.label);
        }
        if (!last) {
            const QPointF center(segment.rect.right() + CHEVRON_WIDTH / 2.0, middle + 0.5);
            painter.setPen(QPen(style::TEXT_TERTIARY, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPolyline(QPolygonF({center + QPointF(-1.8, -3.5), center + QPointF(1.8, 0), center + QPointF(-1.8, 3.5)}));
        }
    }
}

void PathBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutSegments();
}

void PathBar::mouseMoveEvent(QMouseEvent *event)
{
    const int hover = segmentAt(event->position());
    if (hover != m_hover) {
        m_hover = hover;
        setCursor(hover >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void PathBar::mousePressEvent(QMouseEvent *event)
{
    const int index = segmentAt(event->position());
    if (event->button() == Qt::LeftButton && index >= 0) {
        Q_EMIT crumbClicked(m_segments.at(index).crumb.url);
        event->accept();
        return;
    }
    // Presses on the empty part of the bar fall through to the window, which moves it.
    event->ignore();
}

void PathBar::leaveEvent(QEvent *event)
{
    m_hover = -1;
    unsetCursor();
    update();
    QWidget::leaveEvent(event);
}

} // namespace fe
