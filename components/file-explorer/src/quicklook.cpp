// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements quicklook.h.

#include "quicklook.h"

#include "core/quicklook.h"
#include "style.h"
#include "toolbar.h"

#include <KIO/Global>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KIO/PreviewJob>

#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMimeDatabase>
#include <QPlainTextEdit>
#include <QScreen>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fe {

namespace {

constexpr int HEADER_HEIGHT = 46;
/// The most of a text file Quick Look reads: enough to read the start, quick for any size.
constexpr qint64 TEXT_LIMIT = 256 * 1024;
constexpr int PREVIEW_SIZE = 512;
constexpr QSize TEXT_SIZE{780, 560};
constexpr QSize PREVIEW_WINDOW{560, 540};
/// The share of the owner window a picture may fill.
constexpr qreal IMAGE_SHARE = 0.8;

/// The MIME type of item followed by every type it inherits from, for fe::lookKindFor.
QStringList mimeAncestry(const KFileItem &item)
{
    const QMimeType type = item.determineMimeType();
    QStringList ancestry{type.name()};
    ancestry += type.allAncestors();
    return ancestry;
}

} // namespace

QuickLook::QuickLook(QWidget *owner)
    : GlassWindow(owner)
    , m_owner(owner)
    , m_title(new QLabel)
    , m_pages(new QStackedWidget)
    , m_image(new QLabel)
    , m_text(new QPlainTextEdit)
    , m_preview(new QLabel)
    , m_details(new QLabel)
{
    setWindowTitle(QStringLiteral("Quick Look"));
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto *header = new QWidget;
    header->setFixedHeight(HEADER_HEIGHT);
    m_title->setFont(style::uiFont(13, QFont::DemiBold));
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 236);"));
    auto *open = new GlassButton(QString(), QStringLiteral("Open in its application (Enter)"));
    open->setStandalone(true);
    open->setLabel(QStringLiteral("Open"));
    auto *close = new GlassButton(QStringLiteral("window-close-symbolic"), QStringLiteral("Close (Space)"));
    close->setStandalone(true);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(10, 6, 10, 0);
    headerLayout->addWidget(close);
    headerLayout->addWidget(m_title, 1);
    headerLayout->addWidget(open);
    connect(close, &QAbstractButton::clicked, this, &QWidget::close);
    connect(open, &QAbstractButton::clicked, this, &QuickLook::openItem);

    m_image->setAlignment(Qt::AlignCenter);
    m_text->setReadOnly(true);
    m_text->setFrameShape(QFrame::NoFrame);
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_text->setStyleSheet(QStringLiteral("QPlainTextEdit { background: transparent; color: rgba(255, 255, 255, 230); }"));
    auto *previewPage = new QWidget;
    auto *previewLayout = new QVBoxLayout(previewPage);
    m_preview->setAlignment(Qt::AlignCenter);
    m_details->setAlignment(Qt::AlignCenter);
    m_details->setFont(style::uiFont(13));
    m_details->setStyleSheet(QStringLiteral("color: rgba(235, 235, 245, 170);"));
    previewLayout->addWidget(m_preview, 1);
    previewLayout->addWidget(m_details);
    previewLayout->addSpacing(14);
    m_pages->addWidget(m_image);
    m_pages->addWidget(m_text);
    m_pages->addWidget(previewPage);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 0, 6, 10);
    layout->setSpacing(4);
    layout->addWidget(header);
    layout->addWidget(m_pages, 1);
    addDragSurface(header);
}

KFileItem QuickLook::item() const
{
    return m_item;
}

void QuickLook::showItem(const KFileItem &item)
{
    m_item = item;
    m_title->setText(item.text());
    const QString path = item.localPath();
    switch (path.isEmpty() ? LookKind::Preview : lookKindFor(mimeAncestry(item))) {
    case LookKind::Image:
        showImage(path);
        break;
    case LookKind::Text:
        showText(path);
        break;
    case LookKind::Preview:
        showPreview();
        break;
    }
    show();
    raise();
    activateWindow();
}

void QuickLook::showImage(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize full = reader.size();
    const QSize room = (QSizeF(m_owner->size()) * IMAGE_SHARE).toSize() - QSize(12, HEADER_HEIGHT + 14);
    const QSize fitted = full.isValid() ? full.scaled(room, Qt::KeepAspectRatio).boundedTo(full) : room;
    const qreal dpr = devicePixelRatioF();
    reader.setScaledSize(fitted * dpr);
    QImage image = reader.read();
    if (image.isNull()) {
        // A picture Qt cannot read (a raw photo, a layered drawing): the thumbnailers may.
        showPreview();
        return;
    }
    image.setDevicePixelRatio(dpr);
    m_image->setPixmap(QPixmap::fromImage(image));
    m_pages->setCurrentWidget(m_image);
    place(fitted);
}

void QuickLook::showText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        showPreview();
        return;
    }
    m_text->setPlainText(QString::fromUtf8(file.read(TEXT_LIMIT)));
    m_pages->setCurrentWidget(m_text);
    place(TEXT_SIZE);
}

void QuickLook::showPreview()
{
    const qreal dpr = devicePixelRatioF();
    m_preview->setPixmap(QIcon::fromTheme(m_item.iconName()).pixmap(QSize(256, 256), dpr));
    const QString size = m_item.isDir() ? QStringLiteral("Folder") : KIO::convertSize(m_item.size());
    m_details->setText(QStringLiteral("%1\n%2, modified %3")
                           .arg(m_item.mimeComment(), size,
                                QLocale().toString(m_item.time(KFileItem::ModificationTime), QLocale::ShortFormat)));
    m_pages->setCurrentIndex(2);
    place(PREVIEW_WINDOW);

    // A real preview replaces the icon when a thumbnailer knows the type (PDF, video, fonts, ...).
    const QStringList plugins = KIO::PreviewJob::availablePlugins();
    auto *job = KIO::filePreview(KFileItemList{m_item}, QSize(PREVIEW_SIZE, PREVIEW_SIZE) * dpr, &plugins);
    const KFileItem asked = m_item;
    connect(job, &KIO::PreviewJob::gotPreview, this, [this, asked](const KFileItem &got, const QPixmap &pixmap) {
        if (got.url() == m_item.url() && asked.url() == m_item.url()) {
            QPixmap scaled = pixmap;
            scaled.setDevicePixelRatio(devicePixelRatioF());
            m_preview->setPixmap(scaled);
        }
    });
}

void QuickLook::openItem()
{
    auto *job = new KIO::OpenUrlJob(m_item.targetUrl());
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_owner));
    job->start();
    close();
}

void QuickLook::place(QSize content)
{
    const QSize size = content + QSize(12, HEADER_HEIGHT + 14);
    resize(size.expandedTo(QSize(420, 320)));
    const QPoint center = m_owner->geometry().center();
    move(center - QPoint(width() / 2, height() / 2));
}

void QuickLook::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
    case Qt::Key_Escape:
        close();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        openItem();
        return;
    default:
        GlassWindow::keyPressEvent(event);
    }
}

} // namespace fe
