// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements searchscopebar.h.

#include "searchscopebar.h"

#include "style.h"

#include <QMouseEvent>
#include <QPainter>

namespace fe {

namespace {

constexpr int BAR_HEIGHT = 34;
constexpr qreal LEFT = 14.0;
constexpr qreal GAP = 6.0;
constexpr qreal PADDING = 12.0;
constexpr qreal CAPSULE_HEIGHT = 24.0;
const QString LABEL = QStringLiteral("Search:");
const QString EVERYWHERE = QStringLiteral("This PC");
const QString SAVE = QStringLiteral("Save");
constexpr qreal RIGHT = 14.0;

} // namespace

SearchScopeBar::SearchScopeBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(BAR_HEIGHT);
    setFont(style::uiFont(12));
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(QStringLiteral("Search scope"));
}

void SearchScopeBar::setFolderShown(bool shown)
{
    m_folderShown = shown;
    update();
}

void SearchScopeBar::setScope(const QString &folderName, bool everywhere)
{
    m_folderName = folderName;
    m_everywhere = everywhere;
    update();
}

QRectF SearchScopeBar::capsule(int which) const
{
    const QFontMetricsF metrics(font());
    if (which == 2) {
        const qreal saveWidth = metrics.horizontalAdvance(SAVE) + PADDING * 2;
        return {width() - RIGHT - saveWidth, (height() - CAPSULE_HEIGHT) / 2.0, saveWidth, CAPSULE_HEIGHT};
    }
    qreal x = LEFT + metrics.horizontalAdvance(LABEL) + GAP * 2;
    const qreal first = metrics.horizontalAdvance(EVERYWHERE) + PADDING * 2;
    const QString folder = QStringLiteral("\u201C%1\u201D").arg(m_folderName);
    const qreal second = metrics.horizontalAdvance(folder) + PADDING * 2;
    if (which == 1) {
        x += first + GAP;
    }
    return {x, (height() - CAPSULE_HEIGHT) / 2.0, which == 0 ? first : second, CAPSULE_HEIGHT};
}

void SearchScopeBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font());
    painter.setPen(style::TEXT_SECONDARY);
    painter.drawText(QRectF(LEFT, 0, width(), height()), Qt::AlignVCenter | Qt::AlignLeft, LABEL);
    const QStringList labels{EVERYWHERE, QStringLiteral("\u201C%1\u201D").arg(m_folderName)};
    for (int which = 0; which < (m_folderShown ? 2 : 1); ++which) {
        const QRectF rect = capsule(which);
        const bool chosen = (which == 0) == m_everywhere;
        style::paintGlass(painter, rect, rect.height() / 2.0, chosen ? style::SEGMENT_CHECKED : style::CAPSULE_FILL);
        painter.setPen(chosen ? style::TEXT : style::TEXT_SECONDARY);
        painter.drawText(rect, Qt::AlignCenter, labels.at(which));
    }
    const QRectF save = capsule(2);
    style::paintGlass(painter, save, save.height() / 2.0, style::CAPSULE_FILL);
    painter.setPen(style::TEXT);
    painter.drawText(save, Qt::AlignCenter, SAVE);
}

void SearchScopeBar::mousePressEvent(QMouseEvent *event)
{
    if (capsule(2).contains(event->position())) {
        Q_EMIT saveRequested();
        return;
    }
    for (int which = 0; which < (m_folderShown ? 2 : 1); ++which) {
        if (capsule(which).contains(event->position())) {
            const bool everywhere = which == 0;
            if (everywhere != m_everywhere) {
                m_everywhere = everywhere;
                update();
                Q_EMIT scopeChosen(everywhere);
            }
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

} // namespace fe
