// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements gallerystage.h.

#include "gallerystage.h"

#include "style.h"

#include <KIO/PreviewJob>

#include <QIcon>
#include <QPainter>
#include <QTimer>

namespace fe {

namespace {

/// Selection changes this close together, in milliseconds, ask for one preview.
constexpr int REQUEST_MS = 120;
/// The largest preview asked for, in device pixels: enough for a full-width stage on a 4K screen.
constexpr int MAX_PREVIEW = 2048;
constexpr int CAPTION_HEIGHT = 46;
constexpr int MARGIN = 16;

} // namespace

GalleryStage::GalleryStage(QWidget *parent)
    : QWidget(parent)
    , m_request(new QTimer(this))
{
    setAccessibleName(QStringLiteral("Gallery"));
    setMinimumHeight(160);
    m_request->setSingleShot(true);
    m_request->setInterval(REQUEST_MS);
    connect(m_request, &QTimer::timeout, this, &GalleryStage::requestPreview);
    hide();
}

void GalleryStage::setItem(const KFileItem &item)
{
    if (item.url() == m_item.url() && item.time(KFileItem::ModificationTime) == m_item.time(KFileItem::ModificationTime)) {
        return;
    }
    m_item = item;
    m_preview = false;
    m_picture = QPixmap();
    if (m_job) {
        m_job->kill();
    }
    setAccessibleDescription(item.isNull() ? QString() : item.text());
    update();
    if (!item.isNull() && isVisible()) {
        m_request->start();
    }
}

KFileItem GalleryStage::item() const
{
    return m_item;
}

bool GalleryStage::showsPreview() const
{
    return m_preview;
}

void GalleryStage::requestPreview()
{
    if (m_item.isNull() || m_item.isDir() || !isVisible()) {
        return;
    }
    const qreal ratio = devicePixelRatioF();
    const QSize area = (size() - QSize(2 * MARGIN, 2 * MARGIN + CAPTION_HEIGHT)) * ratio;
    const QSize wanted(std::clamp(area.width(), 64, MAX_PREVIEW), std::clamp(area.height(), 64, MAX_PREVIEW));
    auto *job = KIO::filePreview(KFileItemList{m_item}, wanted);
    job->setScaleType(KIO::PreviewJob::Scaled);
    m_job = job;
    const QUrl wantedUrl = m_item.url();
    connect(job, &KIO::PreviewJob::gotPreview, this, [this, wantedUrl](const KFileItem &item, const QPixmap &pixmap) {
        if (item.url() != wantedUrl || m_item.url() != wantedUrl) {
            return;
        }
        m_picture = pixmap;
        m_preview = true;
        update();
        Q_EMIT pictureChanged();
    });
}

void GalleryStage::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRect area = rect().adjusted(MARGIN, MARGIN, -MARGIN, -MARGIN - CAPTION_HEIGHT);
    if (m_item.isNull()) {
        painter.setFont(style::uiFont(14));
        painter.setPen(style::TEXT_SECONDARY);
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Select an item to see it here"));
        return;
    }
    QPixmap picture = m_picture;
    if (picture.isNull()) {
        // Until a preview arrives, or for a kind no thumbnailer knows: the item's icon, large.
        const int side = std::min({area.width(), area.height(), 256});
        picture = QIcon::fromTheme(m_item.iconName()).pixmap(QSize(side, side), devicePixelRatioF());
    }
    const QSize shown = picture.deviceIndependentSize().toSize().scaled(area.size(), Qt::KeepAspectRatio).boundedTo(
        m_preview ? area.size() : picture.deviceIndependentSize().toSize());
    const QRect target(area.left() + (area.width() - shown.width()) / 2, area.top() + (area.height() - shown.height()) / 2,
                       shown.width(), shown.height());
    painter.drawPixmap(target, picture);
    painter.setFont(style::uiFont(14, QFont::DemiBold));
    painter.setPen(style::TEXT);
    const QRect caption(rect().left(), rect().bottom() - MARGIN - CAPTION_HEIGHT + 4, width(), 22);
    painter.drawText(caption, Qt::AlignHCenter | Qt::AlignVCenter, painter.fontMetrics().elidedText(m_item.text(), Qt::ElideMiddle, width() - 2 * MARGIN));
    painter.setFont(style::uiFont(12));
    painter.setPen(style::TEXT_SECONDARY);
    painter.drawText(caption.translated(0, 20), Qt::AlignHCenter | Qt::AlignVCenter, m_item.mimeComment());
}

void GalleryStage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // A larger stage wants a larger picture; a smaller one scales down the one it has.
    if (m_preview && !m_picture.isNull() && (width() * devicePixelRatioF() > m_picture.width() * 1.25)) {
        m_request->start();
    }
}

void GalleryStage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_item.isNull() && !m_preview) {
        m_request->start();
    }
}

void GalleryStage::hideEvent(QHideEvent *event)
{
    m_request->stop();
    if (m_job) {
        m_job->kill();
    }
    QWidget::hideEvent(event);
}

} // namespace fe
