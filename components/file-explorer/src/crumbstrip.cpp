// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements crumbstrip.h.

#include "crumbstrip.h"

#include "style.h"
#include "tagging.h"

#include <KIO/DropJob>
#include <KJobWidgets>

#include <QAccessible>
#include <QApplication>
#include <KProtocolInfo>
#include <KProtocolManager>

#include <QCollator>
#include <QMimeDatabase>
#include <QContextMenuEvent>
#include <QDirListing>
#include <QDrag>
#include <QFileInfo>
#include <QIcon>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWidget>

namespace fe {

namespace {

constexpr int CRUMB_ICON_SIZE = 14;
constexpr qreal CRUMB_ICON_GAP = 5.0;
/// The room between two labels, where the chevron is drawn and clicked.
constexpr qreal SEPARATOR = 16.0;
/// Grace around a label for the pointer, in pixels.
constexpr qreal SLOP = 3.0;
/// How long a drag rests on a segment before it opens: long enough that sweeping across the bar on the way to the files
/// opens nothing.
constexpr int SPRING_LOAD_MS = 700;
/// The most folders a chevron's menu lists; a longer menu ends with an item that opens the folder instead.
constexpr int FOLDER_MENU_LIMIT = 250;
constexpr int DRAG_ICON_SIZE = 32;
const QString ELLIPSIS = QStringLiteral("\u2026");

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
    if (!crumb.url.isLocalFile() && !KProtocolInfo::archiveMimetypes(scheme).isEmpty()) {
        // In an archive: the archive itself shows its kind's icon, the folders in it a folder. The name decides, so a
        // paint never reads the disk.
        const QMimeType type = QMimeDatabase().mimeTypeForFile(crumb.url.path(), QMimeDatabase::MatchExtension);
        return KProtocolManager::protocolForArchiveMimetype(type.name()).isEmpty() ? QStringLiteral("folder") : type.iconName();
    }
    if (!crumb.url.isLocalFile()) {
        return QStringLiteral("folder-network");
    }
    if (first) {
        return crumb.url.path() == u"/" ? QStringLiteral("drive-harddisk") : QStringLiteral("user-home");
    }
    return QStringLiteral("folder");
}

/// The visible folders inside a local folder, in the order a person sorts names ("2" before "10").
QStringList subfolderNames(const QString &path)
{
    QStringList names;
    // Hidden folders stay out, as the view leaves them out by default; links to folders count as folders.
    for (const QDirListing::DirEntry &entry :
         QDirListing(path, QDirListing::IteratorFlag::DirsOnly | QDirListing::IteratorFlag::ResolveSymlinks)) {
        names.append(entry.fileName());
    }
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(names.begin(), names.end(), collator);
    return names;
}

} // namespace

CrumbStrip::CrumbStrip(QWidget *host, bool showIcons, std::optional<Qt::CursorShape> restCursor)
    : QObject(host)
    , m_host(host)
    , m_showIcons(showIcons)
    , m_restCursor(restCursor)
    , m_springTimer(new QTimer(this))
{
    m_springTimer->setSingleShot(true);
    m_springTimer->setInterval(SPRING_LOAD_MS);
    connect(m_springTimer, &QTimer::timeout, this, [this] {
        if (const Crumb *crumb = crumbOf(m_dropSlot)) {
            // A copy: opening the folder replaces the segments, and with them the URL a reference would point into.
            const QUrl url = crumb->url;
            Q_EMIT crumbActivated(url, CrumbOpen::Here);
        }
    });
}

void CrumbStrip::setCrumbs(const QList<Crumb> &crumbs, const QString &lastIconName, bool lastIsFolder)
{
    m_crumbs = crumbs;
    m_lastIconName = lastIconName;
    m_lastIsFolder = lastIsFolder;
    m_hover = {};
    m_pressSlot = -1;
    // A drag that opened a segment goes on over the new path; its next move finds its target again.
    m_dropSlot = -1;
    m_springTimer->stop();
    relayout();
}

void CrumbStrip::setTagsPath(const QString &tagsPath)
{
    m_tagsPath = tagsPath;
}

void CrumbStrip::setArea(const QRectF &area)
{
    if (area != m_area) {
        m_area = area;
        relayout();
    }
}

bool CrumbStrip::isEmpty() const
{
    return m_crumbs.isEmpty();
}

void CrumbStrip::relayout()
{
    const QFontMetricsF metrics(m_host->font());
    const qreal iconWidth = m_showIcons ? CRUMB_ICON_SIZE + CRUMB_ICON_GAP : 0.0;
    QList<qreal> widths;
    widths.reserve(m_crumbs.size());
    for (const Crumb &crumb : std::as_const(m_crumbs)) {
        widths.append(iconWidth + metrics.horizontalAdvance(crumb.label));
    }
    m_layout = layoutCrumbs(widths, m_area.left(), SEPARATOR, metrics.horizontalAdvance(ELLIPSIS), m_area.width());
    ++m_generation;
    if (QAccessible::isActive()) {
        QAccessibleEvent reorder(m_host, QAccessible::ObjectReorder);
        QAccessible::updateAccessibility(&reorder);
    }
    m_host->update();
}

qsizetype CrumbStrip::slotCount() const
{
    return m_layout.cells.size();
}

QString CrumbStrip::slotName(qsizetype slot) const
{
    if (slot < 0 || slot >= m_layout.cells.size()) {
        return {};
    }
    const Crumb *crumb = crumbOf(slot);
    return crumb ? crumb->label : QStringLiteral("More folders");
}

QRectF CrumbStrip::slotRect(qsizetype slot) const
{
    if (slot < 0 || slot >= m_layout.cells.size()) {
        return {};
    }
    const CrumbSlot &cell = m_layout.cells.at(slot);
    return {cell.x, m_area.top(), cell.width, m_area.height()};
}

void CrumbStrip::activateSlot(qsizetype slot)
{
    if (slot < 0 || slot >= m_layout.cells.size()) {
        return;
    }
    if (const Crumb *crumb = crumbOf(slot)) {
        const QUrl url = crumb->url;
        Q_EMIT crumbActivated(url, CrumbOpen::Here);
    } else {
        showFoldedMenu(slot);
    }
}

quint64 CrumbStrip::generation() const
{
    return m_generation;
}

CrumbHit CrumbStrip::hitAt(QPointF pos) const
{
    if (pos.y() < m_area.top() || pos.y() > m_area.bottom()) {
        return {};
    }
    return crumbHitAt(m_layout, pos.x(), SEPARATOR, SLOP);
}

const Crumb *CrumbStrip::crumbOf(qsizetype slot) const
{
    if (slot < 0 || slot >= m_layout.cells.size()) {
        return nullptr;
    }
    const qsizetype index = m_layout.cells.at(slot).crumb;
    return index < 0 || index >= m_crumbs.size() ? nullptr : &m_crumbs.at(index);
}

bool CrumbStrip::isFolderSlot(qsizetype slot) const
{
    const bool last = slot >= 0 && m_layout.cells.at(slot).crumb == m_crumbs.size() - 1;
    return !last || m_lastIsFolder;
}

bool CrumbStrip::isOverCrumb(QPointF pos) const
{
    return hitAt(pos).part != CrumbPart::None;
}

void CrumbStrip::paint(QPainter &painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(m_host->font());
    const QFontMetricsF metrics(m_host->font());
    const qreal dpr = m_host->devicePixelRatioF();
    const qreal middle = m_area.center().y();

    for (qsizetype i = 0; i < m_layout.cells.size(); ++i) {
        const CrumbSlot &slot = m_layout.cells.at(i);
        const bool last = i == m_layout.cells.size() - 1;
        const bool hovered = m_hover.slot == i && m_hover.part == CrumbPart::Label;
        const QRectF label(slot.x, m_area.top(), slot.width, m_area.height());

        if (m_dropSlot == i) {
            // The folder that takes the drop, in the accent, as the sidebar marks a place under a drag.
            QColor target = style::ACCENT;
            target.setAlpha(110);
            painter.setPen(Qt::NoPen);
            painter.setBrush(target);
            painter.drawRoundedRect(QRectF(slot.x - 5, middle - 10, slot.width + 10, 20), 6, 6);
        }

        const Crumb *crumb = crumbOf(i);
        if (!crumb) {
            painter.setPen(hovered ? style::TEXT : style::TEXT_SECONDARY);
            painter.drawText(label, Qt::AlignVCenter | Qt::AlignLeft, ELLIPSIS);
        } else {
            qreal textLeft = slot.x;
            if (m_showIcons) {
                const QString iconName = last && !m_lastIconName.isEmpty() ? m_lastIconName : crumbIcon(*crumb, slot.crumb == 0);
                const QPixmap icon = QIcon::fromTheme(iconName, QIcon::fromTheme(QStringLiteral("folder")))
                                         .pixmap(QSize(CRUMB_ICON_SIZE, CRUMB_ICON_SIZE), dpr);
                painter.drawPixmap(QPointF(slot.x, middle - CRUMB_ICON_SIZE / 2.0), icon);
                textLeft += CRUMB_ICON_SIZE + CRUMB_ICON_GAP;
            }
            const qreal textWidth = std::max(0.0, slot.x + slot.width - textLeft);
            painter.setPen(last || hovered || m_dropSlot == i ? style::TEXT : style::TEXT_SECONDARY);
            painter.drawText(QRectF(textLeft, m_area.top(), textWidth, m_area.height()), Qt::AlignVCenter | Qt::AlignLeft,
                             metrics.elidedText(crumb->label, Qt::ElideMiddle, textWidth));
        }

        if (!last) {
            const QPointF center(slot.x + slot.width + SEPARATOR / 2.0, middle + 0.5);
            const bool chevronHovered = m_hover.slot == i && m_hover.part == CrumbPart::Chevron;
            if (chevronHovered) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(style::HOVER);
                painter.drawRoundedRect(QRectF(center.x() - 6, middle - 9, 12, 18), 4, 4);
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(chevronHovered ? style::TEXT : style::TEXT_TERTIARY, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPolyline(QPolygonF({center + QPointF(-1.8, -3.5), center + QPointF(1.8, 0), center + QPointF(-1.8, 3.5)}));
        }
    }
    painter.restore();
}

QString CrumbStrip::toolTipAt(QPointF pos) const
{
    const CrumbHit hit = hitAt(pos);
    if (hit.part == CrumbPart::None) {
        return {};
    }
    const Crumb *crumb = crumbOf(hit.slot);
    if (hit.part == CrumbPart::Chevron && crumb) {
        return QStringLiteral("Folders in %1").arg(crumb->label);
    }
    if (crumb) {
        return crumb->url.toDisplayString(QUrl::PreferLocalFile);
    }
    QStringList folded;
    for (const qsizetype index : m_layout.folded) {
        folded.append(m_crumbs.at(index).label);
    }
    return folded.join(QStringLiteral(" \u203a "));
}

void CrumbStrip::setHover(CrumbHit hover)
{
    if (hover.slot == m_hover.slot && hover.part == m_hover.part) {
        return;
    }
    m_hover = hover;
    if (hover.part != CrumbPart::None) {
        m_host->setCursor(Qt::PointingHandCursor);
    } else if (m_restCursor) {
        m_host->setCursor(*m_restCursor);
    } else {
        m_host->unsetCursor();
    }
    m_host->update();
}

void CrumbStrip::setDropSlot(qsizetype slot)
{
    if (slot == m_dropSlot) {
        return;
    }
    m_dropSlot = slot;
    // The folder on screen is already open; every other folder opens when the drag rests on it.
    const bool onScreen = slot >= 0 && m_layout.cells.at(slot).crumb == m_crumbs.size() - 1 && m_lastIsFolder;
    if (slot >= 0 && !onScreen) {
        m_springTimer->start();
    } else {
        m_springTimer->stop();
    }
    m_host->update();
}

bool CrumbStrip::mousePress(QMouseEvent *event)
{
    const CrumbHit hit = hitAt(event->position());
    if (hit.part == CrumbPart::None) {
        return false;
    }
    if (event->button() == Qt::LeftButton && hit.part == CrumbPart::Chevron) {
        showFolderMenu(hit.slot);
    } else if (event->button() == Qt::LeftButton && !crumbOf(hit.slot)) {
        showFoldedMenu(hit.slot);
    } else if ((event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) && crumbOf(hit.slot)) {
        // The click lands on release, so the press can still become a drag.
        m_pressSlot = hit.slot;
        m_pressPos = event->position();
    }
    event->accept();
    return true;
}

bool CrumbStrip::mouseMove(QMouseEvent *event)
{
    if (m_pressSlot >= 0 && event->buttons().testFlag(Qt::LeftButton)
        && (event->position() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
        const qsizetype slot = m_pressSlot;
        m_pressSlot = -1;
        startDrag(slot);
        return true;
    }
    const CrumbHit hit = hitAt(event->position());
    setHover(hit);
    return hit.part != CrumbPart::None || m_pressSlot >= 0;
}

bool CrumbStrip::mouseRelease(QMouseEvent *event)
{
    const qsizetype pressed = m_pressSlot;
    m_pressSlot = -1;
    if (pressed < 0) {
        return isOverCrumb(event->position());
    }
    const CrumbHit hit = hitAt(event->position());
    const Crumb *crumb = crumbOf(pressed);
    if (crumb && hit.slot == pressed && hit.part == CrumbPart::Label) {
        const bool newTab = event->button() == Qt::MiddleButton
            || (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ControlModifier));
        const QUrl url = crumb->url;
        Q_EMIT crumbActivated(url, newTab ? CrumbOpen::NewTab : CrumbOpen::Here);
    }
    event->accept();
    return true;
}

void CrumbStrip::leave()
{
    setHover({});
}

bool CrumbStrip::contextMenu(QContextMenuEvent *event)
{
    const CrumbHit hit = hitAt(event->pos());
    const Crumb *crumb = hit.part == CrumbPart::Label ? crumbOf(hit.slot) : nullptr;
    if (!crumb) {
        return hit.part != CrumbPart::None;
    }
    const QUrl url = crumb->url;
    Q_EMIT crumbMenuRequested(url, event->globalPos());
    event->accept();
    return true;
}

void CrumbStrip::showFolderMenu(qsizetype slot)
{
    const Crumb *crumb = crumbOf(slot);
    if (!crumb || !crumb->url.isLocalFile()) {
        return;
    }
    const QString folder = crumb->url.toLocalFile();
    const QStringList names = subfolderNames(folder);
    // The next segment of the path is the folder this one leads to; it reads in bold, as the open one.
    const Crumb *next = crumbOf(slot + 1);
    const QString nextName = next && next->url.isLocalFile() ? QFileInfo(next->url.toLocalFile()).fileName() : QString();

    QMenu menu(m_host);
    const QIcon folderIcon = QIcon::fromTheme(QStringLiteral("folder"));
    for (qsizetype i = 0; i < names.size() && i < FOLDER_MENU_LIMIT; ++i) {
        const QString name = names.at(i);
        QAction *action = menu.addAction(folderIcon, name);
        if (name == nextName) {
            QFont bold = action->font();
            bold.setBold(true);
            action->setFont(bold);
        }
        const QUrl url = QUrl::fromLocalFile(folder + (folder.endsWith(u'/') ? QString() : QStringLiteral("/")) + name);
        connect(action, &QAction::triggered, this, [this, url] { Q_EMIT crumbActivated(url, CrumbOpen::Here); });
    }
    if (names.isEmpty()) {
        menu.addAction(QStringLiteral("No Folders"))->setEnabled(false);
    } else if (names.size() > FOLDER_MENU_LIMIT) {
        menu.addSeparator();
        const QUrl url = crumb->url;
        menu.addAction(QStringLiteral("Show All %1 Folders").arg(names.size()), this,
                       [this, url] { Q_EMIT crumbActivated(url, CrumbOpen::Here); });
    }
    const CrumbSlot &at = m_layout.cells.at(slot);
    menu.exec(m_host->mapToGlobal(QPointF(at.x + at.width, m_area.bottom() + 4).toPoint()));
}

void CrumbStrip::showFoldedMenu(qsizetype slot)
{
    QMenu menu(m_host);
    for (const qsizetype index : std::as_const(m_layout.folded)) {
        const Crumb crumb = m_crumbs.at(index);
        menu.addAction(QIcon::fromTheme(crumbIcon(crumb, index == 0)), crumb.label, this,
                       [this, url = crumb.url] { Q_EMIT crumbActivated(url, CrumbOpen::Here); });
    }
    const CrumbSlot &at = m_layout.cells.at(slot);
    menu.exec(m_host->mapToGlobal(QPointF(at.x, m_area.bottom() + 4).toPoint()));
}

void CrumbStrip::startDrag(qsizetype slot)
{
    const Crumb *crumb = crumbOf(slot);
    if (!crumb) {
        return;
    }
    const bool last = m_layout.cells.at(slot).crumb == m_crumbs.size() - 1;
    const QString iconName = last && !m_lastIconName.isEmpty() ? m_lastIconName : crumbIcon(*crumb, m_layout.cells.at(slot).crumb == 0);
    auto *mime = new QMimeData;
    mime->setUrls({crumb->url});
    auto *drag = new QDrag(m_host);
    drag->setMimeData(mime);
    drag->setPixmap(QIcon::fromTheme(iconName, QIcon::fromTheme(QStringLiteral("folder")))
                        .pixmap(QSize(DRAG_ICON_SIZE, DRAG_ICON_SIZE), m_host->devicePixelRatioF()));
    drag->setHotSpot(QPoint(DRAG_ICON_SIZE / 2, DRAG_ICON_SIZE / 2));
    setHover({});
    // Where it lands decides: KIO asks Move, Copy or Link over a folder, the sidebar links it, a terminal types it.
    drag->exec(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, Qt::CopyAction);
}

CrumbDrop CrumbStrip::dropAt(qsizetype slot, const QDropEvent *event) const
{
    const Crumb *crumb = crumbOf(slot);
    if (!crumb || !event->mimeData()->hasUrls()) {
        return CrumbDrop::Refused;
    }
    const CrumbDrop drop = crumbDrop(crumb->url, isFolderSlot(slot), event->mimeData()->urls(), m_tagsPath);
    if (drop == CrumbDrop::Transfer && crumb->url.isLocalFile() && !QFileInfo(crumb->url.toLocalFile()).isWritable()) {
        return CrumbDrop::Refused;
    }
    return drop;
}

bool CrumbStrip::dragEnter(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        return false;
    }
    event->acceptProposedAction();
    return true;
}

bool CrumbStrip::dragMove(QDragMoveEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        return false;
    }
    const CrumbHit hit = hitAt(event->position());
    const qsizetype slot = hit.part == CrumbPart::Label ? hit.slot : -1;
    const CrumbDrop drop = dropAt(slot, event);
    setDropSlot(drop == CrumbDrop::Refused ? -1 : slot);
    switch (drop) {
    case CrumbDrop::Refused:
        event->ignore();
        break;
    case CrumbDrop::Transfer:
        event->acceptProposedAction();
        break;
    case CrumbDrop::Tag:
        event->setDropAction(Qt::LinkAction);
        event->accept();
        break;
    }
    return true;
}

void CrumbStrip::dragLeave()
{
    setDropSlot(-1);
}

bool CrumbStrip::drop(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        return false;
    }
    const qsizetype slot = m_dropSlot;
    const CrumbDrop drop = dropAt(slot, event);
    const Crumb *crumb = crumbOf(slot);
    const QUrl destination = crumb ? crumb->url : QUrl();
    setDropSlot(-1);
    switch (drop) {
    case CrumbDrop::Refused:
        event->ignore();
        break;
    case CrumbDrop::Transfer: {
        // The sidebar's way, which is Dolphin's (src/views/draganddrophelper.cpp, v26.08.2): KIO asks Move, Copy or
        // Link, and a drop on the Trash moves the files there (KIO src/widgets/dropjob.cpp, v6.31.0, line 554).
        KIO::DropJob *job = KIO::drop(event, destination);
        // KIO opens its menu at the drop's position mapped from this widget (src/widgets/dropjob.cpp, v6.31.0, line
        // 671), and the position is the host's own, so the host goes here, not its window.
        KJobWidgets::setWindow(job, m_host);
        event->acceptProposedAction();
        break;
    }
    case CrumbDrop::Tag:
        tagging::setTag(event->mimeData()->urls(), QFileInfo(destination.toLocalFile()).fileName(), true);
        event->setDropAction(Qt::LinkAction);
        event->accept();
        break;
    }
    return true;
}

} // namespace fe
