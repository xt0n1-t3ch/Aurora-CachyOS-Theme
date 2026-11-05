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

constexpr int CRUMB_ICON_SIZE = 14;
constexpr qreal CRUMB_ICON_GAP = 5.0;
constexpr qreal CHEVRON_WIDTH = 16.0;
constexpr qreal PATH_LEFT = 14.0;
/// Space kept free between the path and the status text at the right end.
constexpr qreal STATUS_GAP = 24.0;
constexpr qreal STATUS_RIGHT = 14.0;

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

PathBar::PathBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(style::PATHBAR_HEIGHT);
    setMouseTracking(true);
    setFont(style::uiFont(12));
}

void PathBar::setCrumbs(const QList<Crumb> &crumbs, const QString &lastIconName)
{
    m_crumbs = crumbs;
    m_lastIconName = lastIconName;
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
    if (!m_lastIconName.isEmpty() && !m_segments.isEmpty()) {
        m_segments.last().iconName = m_lastIconName;
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
    // Too long for the room the status text leaves: fold the segments between the first and the last into one "…",
    // the outermost first.
    const qreal room = width() - PATH_LEFT - statusWidth();
    while (totalWidth() > room && m_segments.size() > 3) {
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

void PathBar::setStatusText(const QString &text)
{
    if (text != m_status) {
        m_status = text;
        layoutSegments();
        update();
    }
}

qreal PathBar::statusWidth() const
{
    return m_status.isEmpty() ? 0.0 : QFontMetricsF(font()).horizontalAdvance(m_status) + STATUS_GAP + STATUS_RIGHT;
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

    if (!m_status.isEmpty()) {
        painter.setPen(style::TEXT_SECONDARY);
        painter.drawText(QRectF(rect()).adjusted(0, 0, -STATUS_RIGHT, 0), Qt::AlignRight | Qt::AlignVCenter, m_status);
    }

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
