// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements sidebar.h. The places come from KFilePlacesModel so Dolphin shows the same list; drives get their own
// names, symbols and capacity bars.

#include "sidebar.h"

#include "core/motion.h"

#include "contextmenu.h"
#include "core/drives.h"
#include "core/tags.h"
#include "core/tree.h"
#include "style.h"
#include "tagging.h"

#include <KConfig>
#include <KConfigGroup>
#include <KFilePlacesModel>
#include <KFormat>
#include <KIO/DropJob>
#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/Global>
#include <KJobWidgets>
#include <KPropertiesDialog>
#include <KProtocolManager>
#include <KSharedConfig>
#include <Solid/Block>
#include <Solid/Device>
#include <Solid/NetworkShare>
#include <Solid/OpticalDisc>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include <QApplication>
#include <QCollator>
#include <QDirIterator>
#include <QDrag>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QSysInfo>
#include <QTimer>
#include <QVariantAnimation>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace fe {

namespace {

struct StandardFolder {
    QStandardPaths::StandardLocation location;
    const char *icon;
};

// The standard folders that follow Recents in Favorites, each with its outline icon.
constexpr std::array<StandardFolder, 6> STANDARD_FOLDERS{{
    {QStandardPaths::DesktopLocation, "user-desktop-symbolic"},
    {QStandardPaths::DocumentsLocation, "folder-documents-symbolic"},
    {QStandardPaths::DownloadLocation, "folder-download-symbolic"},
    {QStandardPaths::PicturesLocation, "folder-pictures-symbolic"},
    {QStandardPaths::MusicLocation, "folder-music-symbolic"},
    {QStandardPaths::MoviesLocation, "folder-videos-symbolic"},
}};

/// How often a drive's capacity is read again; Dolphin reads every minute (kfileplacesview.cpp, KIO 6.31).
constexpr int FREE_SPACE_INTERVAL_MS = 30000;
/// The drag data of a sidebar row being moved: its row number in the sidebar.
constexpr auto ROW_MIME_TYPE = "application/x-aurora-sidebar-row";

/// The user's order of each section: [Sidebar Order] in file-explorerrc, one list per section.
KConfigGroup sidebarOrderGroup()
{
    return KSharedConfig::openConfig(QStringLiteral("file-explorerrc"))->group(QStringLiteral("Sidebar Order"));
}

QString sectionName(bool favorites)
{
    return favorites ? QStringLiteral("Favorites") : QStringLiteral("Locations");
}

/// The unfolded folders and the folded sections: [Sidebar Tree] in file-explorerrc.
KConfigGroup sidebarTreeGroup()
{
    return KSharedConfig::openConfig(QStringLiteral("file-explorerrc"))->group(QStringLiteral("Sidebar Tree"));
}

/// How far each level of subfolders moves right, and the chevron that unfolds a row.
constexpr int TREE_INDENT = 18;
constexpr int CHEVRON = 12;
constexpr int ROW_ICON_LEFT = 18;
constexpr int TAG_DOT = 11;

/// Where a row's icon starts, after the indent of its depth.
int iconLeft(const QRect &row, int depth)
{
    return row.left() + ROW_ICON_LEFT + depth * TREE_INDENT;
}

/// The chevron of an unfoldable row, left of its icon; also what a click hits to unfold it.
QRect chevronRect(const QRect &row, int depth)
{
    return {iconLeft(row, depth) - CHEVRON - 3, row.top() + (row.height() - CHEVRON) / 2, CHEVRON, CHEVRON};
}

/// The subfolders of a local folder, the way people count (2 before 10), hidden ones left out.
QStringList subfoldersOf(const QString &path)
{
    QStringList names = QDir(path).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(names.begin(), names.end(), collator);
    return names;
}

/// Whether a local folder has a subfolder, stopping at the first one.
bool hasSubfolders(const QString &path)
{
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot);
    return it.hasNext();
}

/// The user's names for drives: [Drive Names] in file-explorerrc, by fe::Drive::key.
KConfigGroup driveNamesGroup()
{
    return KSharedConfig::openConfig(QStringLiteral("file-explorerrc"))->group(QStringLiteral("Drive Names"));
}

/// Sizes as Dolphin's Size column and the status row write them.
QString formatBytes(qint64 bytes)
{
    return KFormat().formatByteSize(static_cast<double>(bytes));
}

/// The physical drive a volume lives on.
const Solid::StorageDrive *storageDriveOf(const Solid::Device &device)
{
    for (Solid::Device parent = device; parent.isValid(); parent = parent.parent()) {
        if (const auto *drive = parent.as<Solid::StorageDrive>()) {
            return drive;
        }
    }
    return nullptr;
}

/// The kind of a drive from what Solid reports; the system disk and network shares are told apart first.
DriveKind driveKindFor(const Solid::Device &device, bool system, bool fixed)
{
    if (system) {
        return DriveKind::System;
    }
    if (device.is<Solid::NetworkShare>()) {
        return DriveKind::Network;
    }
    if (device.is<Solid::OpticalDisc>()) {
        return DriveKind::Optical;
    }
    if (const Solid::StorageDrive *drive = storageDriveOf(device)) {
        switch (drive->driveType()) {
        case Solid::StorageDrive::CdromDrive:
            return DriveKind::Optical;
        case Solid::StorageDrive::SdMmc:
        case Solid::StorageDrive::CompactFlash:
        case Solid::StorageDrive::MemoryStick:
        case Solid::StorageDrive::SmartMedia:
        case Solid::StorageDrive::Xd:
            return DriveKind::Card;
        default:
            break;
        }
        if (drive->isHotpluggable() || drive->isRemovable()) {
            // A flash drive reports removable media; a disk in a USB case does not.
            return drive->isRemovable() ? DriveKind::Stick : DriveKind::External;
        }
        return DriveKind::Internal;
    }
    return fixed ? DriveKind::Internal : DriveKind::External;
}

/// Where a drive's own name is kept: the system disk by its role, any other by what survives a reboot.
QString driveKeyFor(const Solid::Device &device, const QString &mountPath, bool system)
{
    if (system) {
        return QStringLiteral("system");
    }
    if (const auto *volume = device.as<Solid::StorageVolume>(); volume && !volume->uuid().isEmpty()) {
        return QStringLiteral("uuid-") + volume->uuid();
    }
    if (const auto *block = device.as<Solid::Block>(); block && !block->device().isEmpty()) {
        return QStringLiteral("dev-") + QFileInfo(block->device()).fileName();
    }
    return mountPath.isEmpty() ? QStringLiteral("udi-") + device.udi() : QStringLiteral("path-") + mountPath;
}

/// The outline symbol macOS would show for a place, from the place's own (colored) icon name.
QString symbolFor(const QString &iconName)
{
    if (iconName.endsWith(u"-symbolic")) {
        return iconName;
    }
    const QString symbolic = iconName + u"-symbolic";
    if (QIcon::hasThemeIcon(symbolic)) {
        return symbolic;
    }
    if (iconName.startsWith(u"drive-") || iconName.startsWith(u"media-")) {
        return QStringLiteral("drive-harddisk-symbolic");
    }
    if (iconName.contains(u"network") || iconName.contains(u"remote")) {
        return QStringLiteral("network-workgroup-symbolic");
    }
    return QStringLiteral("folder-symbolic");
}

/// The theme icon shown in a row menu entry.
QString commandIcon(SidebarCommand command)
{
    switch (command) {
    case SidebarCommand::Open:
        return QStringLiteral("document-open");
    case SidebarCommand::OpenInNewTab:
        return QStringLiteral("tab-new");
    case SidebarCommand::GetInfo:
        return QStringLiteral("document-properties");
    case SidebarCommand::Rename:
        return QStringLiteral("edit-rename");
    case SidebarCommand::Eject:
    case SidebarCommand::Unmount:
        return QStringLiteral("media-eject");
    case SidebarCommand::RemoveFromSidebar:
        return QStringLiteral("list-remove");
    case SidebarCommand::EmptyTrash:
        return QStringLiteral("trash-empty");
    }
    return {};
}

/// Whether the Trash holds nothing, read the way Dolphin reads it (src/trash/dolphintrash.cpp,
/// v26.08.2, line 102): the trash worker keeps this entry up to date.
bool isTrashEmpty()
{
    const KConfig trash(QStringLiteral("trashrc"), KConfig::SimpleConfig);
    return trash.group(QStringLiteral("Status")).readEntry("Empty", true);
}

/// Whether a drag carries only folders on this machine, the only things that can join the sidebar.
bool onlyLocalFolders(const QList<QUrl> &urls)
{
    return !urls.isEmpty()
        && std::all_of(urls.cbegin(), urls.cend(), [](const QUrl &url) { return url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir(); });
}

/// Paints the sidebar rows and edits drive names in place. A drive row shows its capacity bar under its name.
class SidebarDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override
    {
        if (index.data(SidebarModel::IsHeaderRole).toBool()) {
            return {0, style::SECTION_HEIGHT};
        }
        return {0, index.data(SidebarModel::IsDriveRole).toBool() ? style::DRIVE_ROW_HEIGHT : style::ROW_HEIGHT};
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        auto *editor = new QLineEdit(parent);
        editor->setFrame(false);
        editor->setFont(style::uiFont(13));
        // Finder's rename field: the name on a light glass with an accent ring.
        editor->setStyleSheet(QStringLiteral("QLineEdit { background: rgba(255, 255, 255, 30); color: white; border: 1px solid rgb(10, 132, 255);"
                                             " border-radius: 5px; padding: 0 4px; selection-background-color: rgb(10, 132, 255); }"));
        return editor;
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QRect rect = option.rect;
        const int left = rect.left() + 18 + style::SIDEBAR_ICON_SIZE + 8 - 5;
        const int height = 22;
        const bool drive = index.data(SidebarModel::IsDriveRole).toBool();
        const int top = drive ? rect.top() + 3 : rect.top() + (rect.height() - height) / 2;
        editor->setGeometry(left, top, rect.right() - 10 - left, height);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        auto *line = static_cast<QLineEdit *>(editor);
        line->setText(index.data(Qt::EditRole).toString());
        line->selectAll();
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QLineEdit *>(editor)->text(), Qt::EditRole);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRect rect = option.rect;
        const QString title = index.data(Qt::DisplayRole).toString();

        const qreal dpr = painter->device()->devicePixelRatioF();
        if (index.data(SidebarModel::IsHeaderRole).toBool()) {
            painter->setFont(style::uiFont(11, QFont::DemiBold));
            painter->setPen(style::TEXT_SECONDARY);
            const QRect textRect = rect.adjusted(18, 8, -10, 0);
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, title);
            // Finder's section chevron: down while open, right while folded.
            const int chevronX = textRect.left() + painter->fontMetrics().horizontalAdvance(title) + 4;
            QPixmap chevron = style::tintedIcon(QStringLiteral(":/icons/actions/chevron-down.svg"), 10, style::TEXT_SECONDARY, dpr);
            const QPointF center(chevronX + 5, textRect.center().y() + 1);
            painter->translate(center);
            if (!index.data(SidebarModel::ExpandedRole).toBool()) {
                painter->rotate(-90);
            }
            painter->drawPixmap(QPointF(-5, -5), chevron);
            painter->restore();
            return;
        }

        const QRectF pill = QRectF(rect).adjusted(8, 1, -8, -1);
        const bool selected = option.state & QStyle::State_Selected;
        if (selected) {
            QPainterPath path;
            path.addRoundedRect(pill, 7, 7);
            // Gray while the window is inactive, as the Finder's sidebar and the views' selections are.
            painter->fillPath(path, (option.state & QStyle::State_Active) ? style::SELECTION : style::SELECTION_INACTIVE);
        } else if (option.state & QStyle::State_MouseOver) {
            QPainterPath path;
            path.addRoundedRect(pill, 7, 7);
            painter->fillPath(path, style::HOVER);
        }
        const auto *view = qobject_cast<const SidebarView *>(parent());
        if (view && view->dropTargetRow() == index) {
            // The place a drag would drop into lights up, as in Finder's sidebar.
            QColor glow = style::ACCENT;
            glow.setAlpha(90);
            QPainterPath path;
            path.addRoundedRect(pill, 7, 7);
            painter->fillPath(path, glow);
        }
        if (view && view->menuRow() == index) {
            // Finder rings the row whose context menu is open.
            QPainterPath ring;
            ring.addRoundedRect(pill.adjusted(1, 1, -1, -1), 6, 6);
            painter->setPen(QPen(selected ? QColor(255, 255, 255, 220) : style::ACCENT, 2));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(ring);
        }

        const int depth = index.data(SidebarModel::DepthRole).toInt();
        const int left = iconLeft(rect, depth);
        if (index.data(SidebarModel::ExpandableRole).toBool()) {
            const QRect box = chevronRect(rect, depth);
            const QPixmap chevron = style::tintedIcon(QStringLiteral(":/icons/actions/chevron-down.svg"), CHEVRON,
                                                      selected ? QColor(Qt::white) : style::TEXT_SECONDARY, dpr);
            painter->save();
            painter->translate(QRectF(box).center());
            if (!index.data(SidebarModel::ExpandedRole).toBool()) {
                painter->rotate(-90);
            }
            painter->drawPixmap(QPointF(-CHEVRON / 2.0, -CHEVRON / 2.0), chevron);
            painter->restore();
        }
        const QVariant tagColor = index.data(SidebarModel::TagColorRole);
        const int iconTop = rect.top() + (rect.height() - style::SIDEBAR_ICON_SIZE) / 2;
        if (tagColor.isValid()) {
            // A tag shows its color as a dot, the size of Finder's.
            const qreal inset = (style::SIDEBAR_ICON_SIZE - TAG_DOT) / 2.0;
            painter->setPen(selected ? QPen(Qt::white, 1.2) : Qt::NoPen);
            painter->setBrush(tagColor.value<QColor>());
            painter->drawEllipse(QRectF(left + inset, iconTop + inset, TAG_DOT, TAG_DOT));
        } else {
            const QColor iconColor = selected ? QColor(Qt::white) : style::ACCENT;
            const QPixmap icon = style::tintedIcon(index.data(SidebarModel::IconNameRole).toString(), style::SIDEBAR_ICON_SIZE,
                                                   iconColor, dpr);
            painter->drawPixmap(QPoint(left, iconTop), icon);
        }

        painter->setFont(style::uiFont(13));
        painter->setPen(selected ? QColor(Qt::white) : style::TEXT);
        const QRect textRect = rect.adjusted(left - rect.left() + style::SIDEBAR_ICON_SIZE + 8, 0, -14, 0);
        const QString elided = painter->fontMetrics().elidedText(title, Qt::ElideRight, textRect.width());
        const QVariant used = index.data(SidebarModel::CapacityRole);
        if (!used.isValid()) {
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
            painter->restore();
            return;
        }

        // A drive: its name over its capacity bar, the pair centred on the icon. The bar eases thicker under the pointer.
        constexpr qreal BAR = 3.0;
        constexpr qreal BAR_HOVER = 4.5;
        constexpr int GAP = 5;
        const qreal hover = view ? view->hoverProgress(index) : 0;
        const qreal barHeight = BAR + (BAR_HOVER - BAR) * hover;
        const int nameHeight = painter->fontMetrics().height();
        const int top = rect.top() + (rect.height() - nameHeight - GAP - int(BAR_HOVER)) / 2;
        painter->drawText(QRect(textRect.left(), top, textRect.width(), nameHeight), Qt::AlignLeft | Qt::AlignVCenter, elided);

        const QRectF track(textRect.left(), top + nameHeight + GAP + (BAR_HOVER - barHeight) / 2.0, textRect.width(), barHeight);
        painter->setPen(Qt::NoPen);
        painter->setBrush(selected ? QColor(255, 255, 255, 70) : style::CAPACITY_TRACK);
        painter->drawRoundedRect(track, barHeight / 2, barHeight / 2);
        const qreal share = std::clamp(used.toReal(), 0.0, 1.0);
        if (share > 0) {
            QRectF fill = track;
            fill.setWidth(std::max(barHeight, track.width() * share));
            const auto level = static_cast<Capacity::Level>(index.data(SidebarModel::CapacityLevelRole).toInt());
            QColor from = style::ACCENT;
            QColor to(100, 182, 255);
            if (level == Capacity::Level::Low) {
                from = style::CAPACITY_LOW;
                to = QColor(255, 196, 92);
            } else if (level == Capacity::Level::Critical) {
                from = style::CAPACITY_CRITICAL;
                to = QColor(255, 128, 112);
            } else if (selected) {
                from = QColor(255, 255, 255, 235);
                to = QColor(255, 255, 255, 255);
            }
            QLinearGradient gradient(fill.topLeft(), fill.topRight());
            gradient.setColorAt(0, from);
            gradient.setColorAt(1, to);
            painter->setBrush(gradient);
            painter->drawRoundedRect(fill, barHeight / 2, barHeight / 2);
        }
        painter->restore();
    }
};

} // namespace

SidebarModel::SidebarModel(PathContext context, QObject *parent)
    : QAbstractListModel(parent)
    , m_context(std::move(context))
    , m_places(new KFilePlacesModel(this))
    , m_names(driveNamesGroup().entryMap())
    , m_hostName(QSysInfo::machineHostName())
    , m_freeSpaceTimer(new QTimer(this))
{
    const KConfigGroup tree = sidebarTreeGroup();
    const QStringList expanded = tree.readEntry("Expanded", QStringList());
    const QStringList folded = tree.readEntry("Folded Sections", QStringList());
    m_expanded = QSet<QString>(expanded.cbegin(), expanded.cend());
    m_foldedSections = QSet<QString>(folded.cbegin(), folded.cend());
    m_freeSpaceTimer->setInterval(FREE_SPACE_INTERVAL_MS);
    connect(m_freeSpaceTimer, &QTimer::timeout, this, &SidebarModel::refreshFreeSpace);
    m_freeSpaceTimer->start();
    connect(m_places, &QAbstractItemModel::rowsInserted, this, &SidebarModel::rebuild);
    connect(m_places, &QAbstractItemModel::rowsRemoved, this, &SidebarModel::rebuild);
    connect(m_places, &QAbstractItemModel::dataChanged, this, &SidebarModel::rebuild);
    connect(m_places, &QAbstractItemModel::modelReset, this, &SidebarModel::rebuild);
    connect(m_places, &KFilePlacesModel::setupDone, this, [this](const QModelIndex &index, bool success) {
        if (success && index == m_pendingSetup) {
            Q_EMIT placeReady(m_places->url(index), m_pendingTarget);
        }
        m_pendingSetup = QPersistentModelIndex();
    });
    rebuild();
}

const SidebarModel::Row *SidebarModel::rowAt(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return nullptr;
    }
    return &m_rows.at(index.row());
}

void SidebarModel::activate(const QModelIndex &index, OpenIn where)
{
    const Row *row = rowAt(index);
    if (!row) {
        return;
    }
    // A drive that is not mounted yet is mounted first, then opened, like Finder does.
    if (row->place.isValid() && m_places->setupNeeded(row->place)) {
        m_pendingSetup = row->place;
        m_pendingTarget = where;
        m_places->requestSetup(row->place);
        return;
    }
    if (!row->tag.isEmpty()) {
        // The shelf is tidied as it opens: files that lost the tag or are gone leave it.
        Q_EMIT placeReady(tagging::shelfUrl(row->tag), where);
        return;
    }
    if (!row->url.isEmpty()) {
        Q_EMIT placeReady(row->url, where);
    }
}

SidebarPlace SidebarModel::placeAt(const QModelIndex &index) const
{
    const Row *row = rowAt(index);
    if (!row) {
        return {};
    }
    SidebarPlace place{.url = row->url, .addedByUser = row->addedByUser, .renamable = !row->driveKey.isEmpty() || row->addedByUser};
    if (row->place.isValid() && m_places->isDevice(row->place)) {
        place.ejectAllowed = m_places->isEjectAllowed(row->place);
        place.teardownAllowed = m_places->isTeardownAllowed(row->place);
        place.fixedDrive = m_places->data(row->place, KFilePlacesModel::FixedDeviceRole).toBool();
    }
    if (row->url.scheme() == u"trash") {
        place.trashEmpty = isTrashEmpty();
    }
    return place;
}

void SidebarModel::addFolder(const QUrl &url, const QString &title)
{
    // The folder's own icon (KIO reads .directory files and knows the XDG folders); the row draws its outline. A saved
    // search of the index is a smart folder, with the icon KDE gives saved searches.
    m_places->addPlace(title, url, url.scheme() == u"aurora-search" ? QStringLiteral("folder-saved-search") : KIO::iconNameForUrl(url));
}

void SidebarModel::removeFromSidebar(const QModelIndex &index)
{
    const Row *row = rowAt(index);
    if (row && row->addedByUser && row->place.isValid()) {
        m_places->removePlace(row->place);
    }
}

void SidebarModel::eject(const QModelIndex &index)
{
    const Row *row = rowAt(index);
    if (!row || !row->place.isValid()) {
        return;
    }
    // A disc's tray opens; any other drive is unmounted so it can be pulled out, as KDE's Safely Remove does.
    if (m_places->isEjectAllowed(row->place)) {
        m_places->requestEject(row->place);
    } else {
        m_places->requestTeardown(row->place);
    }
}

void SidebarModel::unmount(const QModelIndex &index)
{
    const Row *row = rowAt(index);
    if (row && row->place.isValid()) {
        m_places->requestTeardown(row->place);
    }
}

bool SidebarModel::acceptsFiles(const QModelIndex &index) const
{
    const Row *row = rowAt(index);
    // KIO's answer for each place: the Trash, folders and network shares take files; Recents and Network do not.
    // A tag takes local files to tag them.
    if (row && !row->tag.isEmpty()) {
        return true;
    }
    return row && !row->header && !row->url.isEmpty() && KProtocolManager::supportsWriting(row->url);
}

bool SidebarModel::inFavorites(const QModelIndex &index) const
{
    const Row *row = rowAt(index);
    return row && !row->header && row->depth == 0 && index.row() < m_locationsHeader;
}

QModelIndex SidebarModel::placeIndexFor(const QUrl &url) const
{
    for (int i = 0; i < m_places->rowCount(); ++i) {
        const QModelIndex index = m_places->index(i, 0);
        if (m_places->url(index).matches(url, QUrl::StripTrailingSlash)) {
            return index;
        }
    }
    return {};
}

void SidebarModel::insertFolders(const QList<QUrl> &folders, const QModelIndex &index, bool below)
{
    const QList<QUrl> fresh = foldersToAdd(folders, QUrl(), [this](const QUrl &url) { return indexForUrl(url).isValid(); });
    if (fresh.isEmpty() || !rowAt(index)) {
        return;
    }
    // The saved order takes the new folders where they were dropped; without one, KDE's order below places them.
    QStringList keys = sectionKeys(true);
    qsizetype at = topLevelPosition(index.row()) + (below ? 1 : 0);
    for (const QUrl &url : fresh) {
        keys.insert(std::clamp<qsizetype>(at++, 0, keys.size()), url.adjusted(QUrl::StripTrailingSlash).toString());
    }
    KConfigGroup order = sidebarOrderGroup();
    if (order.hasKey(sectionName(true))) {
        order.writeEntry(sectionName(true), keys);
        order.sync();
    }
    // KDE keeps its places in order (KFilePlacesModel::addPlace, its after argument), and only the
    // folders the user added can move: the new ones follow the last of those above the drop, or go
    // right before the first of them when the drop is above them all.
    const int slot = index.row() + (below ? 1 : 0);
    QModelIndex after;
    for (int row = slot - 1; row > 0 && !m_rows.at(row).header; --row) {
        if (m_rows.at(row).addedByUser && m_rows.at(row).place.isValid()) {
            after = m_rows.at(row).place;
            break;
        }
    }
    if (!after.isValid()) {
        const auto first = std::find_if(m_rows.cbegin(), m_rows.cend(), [](const Row &row) { return row.addedByUser && row.place.isValid(); });
        if (first != m_rows.cend() && first->place.row() > 0) {
            after = m_places->index(first->place.row() - 1, 0);
        }
    }
    for (const QUrl &url : fresh) {
        m_places->addPlace(placeTitle(url, m_context), url, KIO::iconNameForUrl(url), QString(), after);
        // Each one after the one before, so several folders keep the order they were dragged in.
        after = placeIndexFor(url);
    }
}

void SidebarModel::rebuild()
{
    Row recents;
    Row home;
    Row network;
    Row trash;
    QList<Row> bookmarks;
    QList<Row> devices;
    QList<Drive> drives;
    const QString homePath = QDir::cleanPath(m_context.homePath);

    for (int i = 0; i < m_places->rowCount(); ++i) {
        const QModelIndex index = m_places->index(i, 0);
        if (m_places->isHidden(index)) {
            continue;
        }
        const QUrl url = m_places->url(index);
        const QString path = url.isLocalFile() ? QDir::cleanPath(url.toLocalFile()) : QString();
        Row row{m_places->text(index), symbolFor(m_places->data(index, KFilePlacesModel::IconNameRole).toString()), url, false,
                QPersistentModelIndex(index), false, {}, {}};
        switch (m_places->groupType(index)) {
        case KFilePlacesModel::PlacesType:
            if (url.scheme() == u"trash") {
                trash = row;
                const bool full = m_places->data(index, KFilePlacesModel::IconNameRole).toString().contains(u"full");
                trash.iconName = full ? QStringLiteral(":/icons/places/trash-full.svg") : QStringLiteral(":/icons/places/trash.svg");
            } else if (path == homePath) {
                home = row;
                home.title = m_context.userName;
                home.iconName = QStringLiteral(":/icons/places/home.svg");
            } else {
                row.addedByUser = true;
                bookmarks.append(row);
            }
            break;
        case KFilePlacesModel::RecentlySavedType:
            if (url.path().contains(u"files")) {
                recents = row;
                recents.title = QStringLiteral("Recents");
                recents.iconName = QStringLiteral("document-open-recent-symbolic");
            }
            break;
        case KFilePlacesModel::DevicesType:
        case KFilePlacesModel::RemovableDevicesType: {
            // The system disk leads the drives under the computer's name, as Macintosh HD does; each drive
            // shows the name the user gave it and its own symbol.
            const bool system = path == u"/";
            const bool fixed = m_places->data(index, KFilePlacesModel::FixedDeviceRole).toBool();
            const Solid::Device device = m_places->deviceForIndex(index);
            const Drive drive{driveKeyFor(device, path, system), row.title, driveKindFor(device, system, fixed)};
            row.driveKey = drive.key;
            row.defaultTitle = driveName(drive, {}, m_hostName);
            row.title = driveName(drive, m_names, m_hostName);
            row.iconName = driveSymbol(drive.kind);
            drives.append(drive);
            devices.append(row);
            break;
        }
        case KFilePlacesModel::SearchForType:
            // KDE files any place whose scheme holds "search" under Search For (kfileplacesitem.cpp, KIO 6.31.0,
            // line 198); a saved search of the index is a smart folder, which Finder keeps in Favorites. Baloo's
            // searches stay out, as Dolphin's own sidebar hides them without Baloo.
            if (url.scheme() == u"aurora-search") {
                row.addedByUser = true;
                // Breeze has no symbolic smart folder; the magnifier says what the row is.
                row.iconName = QIcon::hasThemeIcon(QStringLiteral("folder-saved-search-symbolic")) ? QStringLiteral("folder-saved-search-symbolic")
                                                                                                 : QStringLiteral("system-search-symbolic");
                bookmarks.append(row);
            }
            break;
        case KFilePlacesModel::RemoteType:
            network = row;
            network.iconName = QStringLiteral(":/icons/places/network.svg");
            break;
        default:
            break;
        }
    }

    // Finder's order: Recents, the standard folders, then the user's own bookmarks.
    QList<Row> favorites;
    if (!recents.url.isEmpty()) {
        favorites.append(recents);
    }
    for (const StandardFolder &standard : STANDARD_FOLDERS) {
        const QString path = QDir::cleanPath(QStandardPaths::writableLocation(standard.location));
        if (path.isEmpty() || path == homePath || !QFileInfo(path).isDir()) {
            continue;
        }
        const QUrl url = QUrl::fromLocalFile(path);
        Row row{QFileInfo(path).fileName(), QString::fromLatin1(standard.icon), url, false, {}, false, {}, {}};
        const auto bookmarked = std::find_if(bookmarks.begin(), bookmarks.end(),
                                             [&url](const Row &other) { return other.url.matches(url, QUrl::StripTrailingSlash); });
        if (bookmarked != bookmarks.end()) {
            row.title = bookmarked->title;
            row.place = bookmarked->place;
            bookmarks.erase(bookmarked);
        }
        favorites.append(row);
    }
    favorites += bookmarks;

    QList<Row> locations;
    if (!home.url.isEmpty()) {
        locations.append(home);
    }
    for (const qsizetype i : driveOrder(drives, m_names, m_hostName)) {
        locations.append(devices.at(i));
    }
    for (const Row *row : {&network, &trash}) {
        if (!row->url.isEmpty()) {
            locations.append(*row);
        }
    }

    // The order the user left by dragging rows, over Finder's.
    const KConfigGroup order = sidebarOrderGroup();
    const auto inSavedOrder = [&order](const QList<Row> &rows, bool favoritesSection) {
        QStringList keys;
        for (const Row &row : rows) {
            keys.append(orderKey(row));
        }
        QList<Row> sorted;
        for (const qsizetype at : sidebarOrder(keys, order.readEntry(sectionName(favoritesSection), QStringList()))) {
            sorted.append(rows.at(at));
        }
        return sorted;
    };
    favorites = inSavedOrder(favorites, true);
    locations = inSavedOrder(locations, false);

    QList<Row> tags;
    for (const ColorTag &tag : colorTags()) {
        Row row{tag.name, {}, QUrl::fromLocalFile(tagging::shelfRoot() + u'/' + tag.name), false, {}, false, {}, {}};
        row.tag = tag.name;
        tags.append(row);
    }

    beginResetModel();
    m_rows.clear();
    appendSection(sectionName(true), favorites, true);
    m_locationsHeader = static_cast<int>(m_rows.size());
    appendSection(sectionName(false), locations, false);
    m_tagsHeader = static_cast<int>(m_rows.size());
    appendSection(QStringLiteral("Tags"), tags, false);
    endResetModel();
    // The bars fill in as KIO answers; a drive that just appeared gets its answer at once.
    QTimer::singleShot(0, this, &SidebarModel::refreshFreeSpace);
}

void SidebarModel::refreshFreeSpace()
{
    for (const Row &row : std::as_const(m_rows)) {
        if (row.driveKey.isEmpty() || !row.url.isLocalFile() || m_freeSpaceAsked.contains(row.driveKey)
            || (row.place.isValid() && m_places->setupNeeded(row.place))) {
            continue;
        }
        m_freeSpaceAsked.insert(row.driveKey);
        // The way Dolphin's places panel asks (kfileplacesview.cpp, KIO 6.31): the file worker reads statvfs.
        KIO::FileSystemFreeSpaceJob *job = KIO::fileSystemFreeSpace(row.url);
        connect(job, &KJob::result, this, [this, job, key = row.driveKey] {
            m_freeSpaceAsked.remove(key);
            if (job->error()) {
                return;
            }
            const FreeSpace fresh{job->size(), job->availableSize()};
            const FreeSpace known = m_freeSpace.value(key);
            if (known.size == fresh.size && known.available == fresh.available) {
                return;
            }
            m_freeSpace.insert(key, fresh);
            for (int i = 0; i < m_rows.size(); ++i) {
                if (m_rows.at(i).driveKey == key) {
                    const QModelIndex at = index(i, 0);
                    Q_EMIT dataChanged(at, at, {CapacityRole, CapacityLevelRole, Qt::ToolTipRole});
                }
            }
        });
    }
}

bool SidebarModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    const Row *row = rowAt(index);
    if (role != Qt::EditRole || !row || row->header) {
        return false;
    }
    const QString name = cleanDriveName(value.toString());
    if (!row->driveKey.isEmpty()) {
        const QString key = row->driveKey;
        const QString fallback = row->defaultTitle;
        KConfigGroup names = driveNamesGroup();
        // Clearing the name, or typing the default, gives the drive its own name back.
        if (name.isEmpty() || name == fallback) {
            m_names.remove(key);
            names.deleteEntry(key);
        } else {
            m_names.insert(key, name);
            names.writeEntry(key, name);
        }
        names.sync();
        rebuild();
        if (key == u"system") {
            Q_EMIT systemNameChanged(m_names.value(key, fallback));
        }
        return true;
    }
    if (row->addedByUser && row->place.isValid() && !name.isEmpty()) {
        // A folder the user added is one of KDE's places: Dolphin shows the new name too.
        m_places->editPlace(row->place, name, row->url, m_places->data(row->place, KFilePlacesModel::IconNameRole).toString());
        return true;
    }
    return false;
}

void SidebarModel::appendSection(const QString &title, const QList<Row> &rows, bool tree)
{
    Row header{title, {}, {}, true, {}, false, {}, {}};
    header.expanded = !m_foldedSections.contains(title);
    m_rows.append(header);
    if (!header.expanded) {
        return;
    }
    for (Row row : rows) {
        // The folders the user added unfold, like the folders of Finder's and Explorer's navigation trees.
        const QString path = row.url.isLocalFile() ? QDir::cleanPath(row.url.toLocalFile()) : QString();
        row.expandable = tree && row.addedByUser && !path.isEmpty() && hasSubfolders(path);
        row.expanded = row.expandable && m_expanded.contains(path);
        m_rows.append(row);
        if (!row.expanded) {
            continue;
        }
        for (const TreeEntry &entry : expandedRows(path, m_expanded, subfoldersOf)) {
            Row child{entry.name, QStringLiteral("folder-symbolic"), QUrl::fromLocalFile(entry.path), false, {}, false, {}, {}};
            child.depth = entry.depth;
            child.expandable = entry.expanded || hasSubfolders(entry.path);
            child.expanded = entry.expanded;
            m_rows.append(child);
        }
    }
}

qsizetype SidebarModel::topLevelPosition(int row) const
{
    const qsizetype first = row < m_locationsHeader ? 1 : (row < m_tagsHeader ? m_locationsHeader + 1 : m_tagsHeader + 1);
    qsizetype position = 0;
    for (qsizetype i = first; i < row && i < m_rows.size(); ++i) {
        if (m_rows.at(i).depth == 0) {
            ++position;
        }
    }
    return position;
}

void SidebarModel::toggleExpanded(const QModelIndex &index)
{
    const Row *row = rowAt(index);
    if (!row || !row->expandable) {
        return;
    }
    const QString path = QDir::cleanPath(row->url.toLocalFile());
    if (row->expanded) {
        m_expanded.remove(path);
    } else {
        m_expanded.insert(path);
    }
    KConfigGroup tree = sidebarTreeGroup();
    tree.writeEntry("Expanded", QStringList(m_expanded.cbegin(), m_expanded.cend()));
    tree.sync();
    rebuild();
}

void SidebarModel::toggleSection(const QModelIndex &index)
{
    const Row *row = rowAt(index);
    if (!row || !row->header) {
        return;
    }
    if (row->expanded) {
        m_foldedSections.insert(row->title);
    } else {
        m_foldedSections.remove(row->title);
    }
    KConfigGroup tree = sidebarTreeGroup();
    tree.writeEntry("Folded Sections", QStringList(m_foldedSections.cbegin(), m_foldedSections.cend()));
    tree.sync();
    rebuild();
}

int SidebarModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant SidebarModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const Row &row = m_rows.at(index.row());
    const auto space = row.driveKey.isEmpty() ? m_freeSpace.cend() : m_freeSpace.constFind(row.driveKey);
    const bool sized = space != m_freeSpace.cend() && space->size > 0;
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return row.title;
    case Qt::ToolTipRole:
        if (sized) {
            // Dolphin's "212 GB free of 1.9 TB (79% used)", set as a small card: name, free of total, used.
            const Capacity capacity = capacityFor(space->size, space->available);
            return QStringLiteral("<b>%1</b><br>%2<br><span style=\"color:#a1a1aa\">%3% used</span>")
                .arg(row.title.toHtmlEscaped(), capacityText(space->size, space->available, formatBytes),
                     QString::number(qRound(capacity.used * 100)));
        }
        return row.title;
    case IsDriveRole:
        return !row.driveKey.isEmpty();
    case CapacityRole:
        return sized ? QVariant(capacityFor(space->size, space->available).used) : QVariant();
    case CapacityLevelRole:
        return sized ? static_cast<int>(capacityFor(space->size, space->available).level) : 0;
    case UrlRole:
        return row.url;
    case IconNameRole:
        return row.iconName;
    case IsHeaderRole:
        return row.header;
    case DepthRole:
        return row.depth;
    case ExpandableRole:
        return row.expandable;
    case ExpandedRole:
        return row.expanded;
    case TagColorRole:
        return row.tag.isEmpty() ? QVariant() : QVariant(colorTag(row.tag)->color);
    default:
        return {};
    }
}

QString SidebarModel::orderKey(const Row &row)
{
    // A drive's address changes as it mounts and unmounts; its key does not.
    return row.driveKey.isEmpty() ? row.url.adjusted(QUrl::StripTrailingSlash).toString() : QStringLiteral("drive:") + row.driveKey;
}

QStringList SidebarModel::sectionKeys(bool favorites) const
{
    const qsizetype first = favorites ? 1 : m_locationsHeader + 1;
    const qsizetype end = favorites ? m_locationsHeader : m_tagsHeader;
    QStringList keys;
    for (qsizetype row = first; row < end; ++row) {
        if (m_rows.at(row).depth == 0) {
            keys.append(orderKey(m_rows.at(row)));
        }
    }
    return keys;
}

bool SidebarModel::sameSection(const QModelIndex &a, const QModelIndex &b) const
{
    const Row *first = rowAt(a);
    const Row *second = rowAt(b);
    // Subfolder rows follow their favorite, and the tags keep Finder's color order.
    return first && second && !first->header && !second->header && first->depth == 0 && second->depth == 0
        && a.row() < m_tagsHeader && b.row() < m_tagsHeader && (a.row() < m_locationsHeader) == (b.row() < m_locationsHeader);
}

void SidebarModel::moveRow(const QModelIndex &from, const QModelIndex &to, bool below)
{
    if (!sameSection(from, to)) {
        return;
    }
    const bool favorites = from.row() < m_locationsHeader;
    const QStringList keys = movedSidebarRow(sectionKeys(favorites), topLevelPosition(from.row()), topLevelPosition(to.row()), below);
    KConfigGroup order = sidebarOrderGroup();
    order.writeEntry(sectionName(favorites), keys);
    order.sync();
    rebuild();
}

Qt::ItemFlags SidebarModel::flags(const QModelIndex &index) const
{
    if (!index.isValid() || m_rows.at(index.row()).header) {
        return Qt::NoItemFlags;
    }
    const Row &row = m_rows.at(index.row());
    const Qt::ItemFlags editable = !row.driveKey.isEmpty() || row.addedByUser ? Qt::ItemIsEditable : Qt::NoItemFlags;
    const Qt::ItemFlags movable = row.depth == 0 && row.tag.isEmpty() ? Qt::ItemIsDragEnabled : Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | movable | editable;
}

QModelIndex SidebarModel::indexForUrl(const QUrl &url) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &row = m_rows.at(i);
        if (!row.header && row.url.matches(url, QUrl::StripTrailingSlash)) {
            return index(i, 0);
        }
    }
    return {};
}

SidebarView::SidebarView(PathContext context, QWidget *parent)
    : QListView(parent)
    , m_model(new SidebarModel(std::move(context), this))
{
    setModel(m_model);
    setItemDelegate(new SidebarDelegate(this));
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setSelectionMode(QAbstractItemView::SingleSelection);
    // Rename opens the editor from the row's menu; a click only opens the place.
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setMouseTracking(true);
    // Drops are handled here, not by the model: KIO needs the drop event itself to ask Move, Copy or Link.
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(false);
    setAttribute(Qt::WA_TranslucentBackground);
    viewport()->setAttribute(Qt::WA_Hover);
    viewport()->setAutoFillBackground(false);
    QPalette transparent = palette();
    transparent.setColor(QPalette::Base, Qt::transparent);
    transparent.setColor(QPalette::Window, Qt::transparent);
    setPalette(transparent);

    connect(this, &QListView::clicked, this, [this](const QModelIndex &index) { m_model->activate(index); });
    connect(m_model, &SidebarModel::placeReady, this, [this](const QUrl &url, OpenIn where) {
        if (where == OpenIn::NewTab) {
            Q_EMIT newTabRequested(url);
        } else {
            Q_EMIT placeActivated(url);
        }
    });
    // The places change on their own (drives come and go); the highlight must survive the rebuild.
    connect(m_model, &QAbstractItemModel::modelReset, this, [this] {
        setCurrentFolder(m_currentFolder);
        m_hoverRow = QPersistentModelIndex();
    });
    connect(m_model, &SidebarModel::systemNameChanged, this, &SidebarView::systemNameChanged);

    m_hoverAnimation = new QVariantAnimation(this);
    m_hoverAnimation->setStartValue(0.0);
    m_hoverAnimation->setEndValue(1.0);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_hoverProgress = value.toReal();
        if (m_hoverRow.isValid()) {
            viewport()->update(visualRect(m_hoverRow));
        }
    });
}

qreal SidebarView::hoverProgress(const QModelIndex &index) const
{
    return index.isValid() && index == m_hoverRow ? m_hoverProgress : 0.0;
}

void SidebarView::setHoverRow(const QModelIndex &index)
{
    const QModelIndex row = index.isValid() && !index.data(SidebarModel::IsHeaderRole).toBool() ? index : QModelIndex();
    // Every place is a click away: the hand says so, the gaps and section titles keep the arrow.
    viewport()->setCursor(row.isValid() ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (row == m_hoverRow) {
        return;
    }
    if (m_hoverRow.isValid()) {
        viewport()->update(visualRect(m_hoverRow));
    }
    m_hoverRow = row;
    m_hoverAnimation->stop();
    // At the desktop's animation speed; with animations off the highlight shows at once.
    const int duration = motion::scaled(style::SIDEBAR_HOVER_MS, motion::speed());
    m_hoverProgress = duration > 0 ? 0.0 : 1.0;
    if (row.isValid() && duration > 0) {
        m_hoverAnimation->setDuration(duration);
        m_hoverAnimation->start();
    } else if (row.isValid()) {
        viewport()->update(visualRect(row));
    }
}

QModelIndex SidebarView::movingRow(const QMimeData *mime) const
{
    if (!mime->hasFormat(QString::fromLatin1(ROW_MIME_TYPE))) {
        return {};
    }
    bool ok = false;
    const int row = mime->data(QString::fromLatin1(ROW_MIME_TYPE)).toInt(&ok);
    return ok ? m_model->index(row, 0) : QModelIndex();
}

void SidebarView::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && index.isValid()) {
        // A section title folds its section; a row's chevron unfolds its subfolders without opening the folder.
        if (index.data(SidebarModel::IsHeaderRole).toBool()) {
            m_model->toggleSection(index);
            return;
        }
        if (index.data(SidebarModel::ExpandableRole).toBool()
            && chevronRect(visualRect(index), index.data(SidebarModel::DepthRole).toInt()).adjusted(-4, -6, 4, 6).contains(event->position().toPoint())) {
            m_model->toggleExpanded(index);
            return;
        }
    }
    const bool place = event->button() == Qt::LeftButton && index.isValid() && !index.data(SidebarModel::IsHeaderRole).toBool()
        && m_model->flags(index).testFlag(Qt::ItemIsDragEnabled);
    m_pressRow = place ? QPersistentModelIndex(index) : QPersistentModelIndex();
    m_pressPos = event->position().toPoint();
    QListView::mousePressEvent(event);
}

void SidebarView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_pressRow.isValid() && event->buttons().testFlag(Qt::LeftButton)
        && (event->position().toPoint() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
        dragPressedRow();
        return;
    }
    QListView::mouseMoveEvent(event);
    setHoverRow(indexAt(event->position().toPoint()));
}

void SidebarView::dragPressedRow()
{
    const QModelIndex row = m_pressRow;
    m_pressRow = QPersistentModelIndex();
    // The row itself follows the pointer, as in Finder.
    const QRect rect = visualRect(row);
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(ROW_MIME_TYPE), QByteArray::number(row.row()));
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(viewport()->grab(rect));
    drag->setHotSpot(m_pressPos - rect.topLeft());
    drag->exec(Qt::MoveAction);
}

bool SidebarView::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::Leave) {
        setHoverRow({});
    }
    return QListView::viewportEvent(event);
}

void SidebarView::setCurrentFolder(const QUrl &url)
{
    m_currentFolder = url;
    const QModelIndex index = m_model->indexForUrl(url);
    if (index.isValid()) {
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    } else {
        selectionModel()->clearSelection();
    }
}

bool SidebarView::showsFolder(const QUrl &url) const
{
    return m_model->indexForUrl(url).isValid();
}

void SidebarView::addFolder(const QUrl &url, const QString &title)
{
    m_model->addFolder(url, title);
}

QModelIndex SidebarView::menuRow() const
{
    return m_menuRow;
}

QModelIndex SidebarView::dropTargetRow() const
{
    return m_drop == SidebarDrop::IntoPlace ? QModelIndex(m_dropRow) : QModelIndex();
}

void SidebarView::setDropTarget(const QModelIndex &index, SidebarDrop drop)
{
    if (m_dropRow == index && m_drop == drop) {
        return;
    }
    m_dropRow = index;
    m_drop = drop;
    viewport()->update();
}

void SidebarView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasFormat(QString::fromLatin1(ROW_MIME_TYPE))) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void SidebarView::dragMoveEvent(QDragMoveEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const QModelIndex index = indexAt(pos);
    SidebarDrop drop = SidebarDrop::Nowhere;
    if (const QModelIndex moving = movingRow(event->mimeData()); moving.isValid()) {
        // A row of the sidebar moves within its section; dropped on itself it stays.
        if (index.isValid() && index != moving) {
            const QRect row = visualRect(index);
            drop = sidebarRowDrop(pos.y() - row.top(), row.height(), m_model->sameSection(moving, index));
        }
        setDropTarget(index, drop);
        if (drop == SidebarDrop::Nowhere) {
            event->ignore();
        } else {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        }
        return;
    }
    if (index.isValid() && !index.data(SidebarModel::IsHeaderRole).toBool()) {
        const QRect row = visualRect(index);
        drop = sidebarDrop(pos.y() - row.top(), row.height(), m_model->inFavorites(index), m_model->acceptsFiles(index),
                           onlyLocalFolders(event->mimeData()->urls()));
    }
    setDropTarget(index, drop);
    switch (drop) {
    case SidebarDrop::Nowhere:
        event->ignore();
        break;
    case SidebarDrop::IntoPlace:
        event->acceptProposedAction();
        break;
    case SidebarDrop::BeforeRow:
    case SidebarDrop::AfterRow:
        // Joining the sidebar only links the folders; nothing moves.
        event->setDropAction(event->possibleActions().testFlag(Qt::LinkAction) ? Qt::LinkAction : Qt::CopyAction);
        event->accept();
        break;
    }
}

void SidebarView::dragLeaveEvent(QDragLeaveEvent *event)
{
    setDropTarget({}, SidebarDrop::Nowhere);
    event->accept();
}

void SidebarView::dropEvent(QDropEvent *event)
{
    const QModelIndex index = m_dropRow;
    const SidebarDrop drop = m_drop;
    const QUrl destination = index.data(SidebarModel::UrlRole).toUrl();
    setDropTarget({}, SidebarDrop::Nowhere);
    if (const QModelIndex moving = movingRow(event->mimeData()); moving.isValid()) {
        if (drop == SidebarDrop::BeforeRow || drop == SidebarDrop::AfterRow) {
            m_model->moveRow(moving, index, drop == SidebarDrop::AfterRow);
            event->setDropAction(Qt::MoveAction);
            event->accept();
        } else {
            event->ignore();
        }
        return;
    }
    switch (drop) {
    case SidebarDrop::Nowhere:
        event->ignore();
        break;
    case SidebarDrop::IntoPlace: {
        // Files dropped on a tag are tagged, not moved, as in Finder.
        if (const QVariant tagColor = index.data(SidebarModel::TagColorRole); tagColor.isValid()) {
            tagging::setTag(event->mimeData()->urls(), index.data(Qt::DisplayRole).toString(), true);
            event->setDropAction(Qt::LinkAction);
            event->accept();
            break;
        }
        // Dolphin's way (src/views/draganddrophelper.cpp, v26.08.2): KIO asks Move, Copy or Link, and a drop
        // on the Trash moves the files there (src/widgets/dropjob.cpp, v6.31.0, line 554).
        KIO::DropJob *job = KIO::drop(event, destination);
        // KIO opens its menu at the drop's position mapped from this widget (src/widgets/dropjob.cpp, v6.31.0, line
        // 671); the position is the viewport's own, so the viewport goes here, not the window.
        KJobWidgets::setWindow(job, viewport());
        event->acceptProposedAction();
        break;
    }
    case SidebarDrop::BeforeRow:
    case SidebarDrop::AfterRow:
        m_model->insertFolders(event->mimeData()->urls(), index, drop == SidebarDrop::AfterRow);
        event->setDropAction(event->possibleActions().testFlag(Qt::LinkAction) ? Qt::LinkAction : Qt::CopyAction);
        event->accept();
        break;
    }
}

void SidebarView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);
    if ((m_drop != SidebarDrop::BeforeRow && m_drop != SidebarDrop::AfterRow) || !m_dropRow.isValid()) {
        return;
    }
    // macOS marks where dragged folders will join the sidebar with a line that starts in a small ring.
    const QRect row = visualRect(m_dropRow);
    const qreal y = m_drop == SidebarDrop::BeforeRow ? row.top() + 0.5 : row.bottom() + 1.5;
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(style::ACCENT, 2, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(row.left() + 16, y), 3.5, 3.5);
    painter.drawLine(QPointF(row.left() + 20, y), QPointF(row.right() - 10, y));
}

QItemSelectionModel::SelectionFlags SidebarView::selectionCommand(const QModelIndex &index, const QEvent *event) const
{
    // A right click only opens the row's menu; the highlight stays on the folder on screen, as in Finder.
    if (event && event->type() == QEvent::MouseButtonPress && static_cast<const QMouseEvent *>(event)->button() != Qt::LeftButton) {
        return QItemSelectionModel::NoUpdate;
    }
    return QListView::selectionCommand(index, event);
}

void SidebarView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid() || index.data(SidebarModel::IsHeaderRole).toBool()) {
        return;
    }
    const QString title = index.data(Qt::DisplayRole).toString();
    QMenu menu(this);
    const QList<QList<SidebarMenuEntry>> groups = sidebarMenu(m_model->placeAt(index));
    for (const QList<SidebarMenuEntry> &group : groups) {
        if (!menu.isEmpty()) {
            menu.addSeparator();
        }
        for (const SidebarMenuEntry &entry : group) {
            QAction *action = menu.addAction(QIcon::fromTheme(commandIcon(entry.command)), sidebarCommandText(entry.command, title));
            action->setEnabled(entry.enabled);
            connect(action, &QAction::triggered, this, [this, command = entry.command, row = QPersistentModelIndex(index)] {
                // The places can change while the menu is open (a drive leaves); a row that is gone does nothing.
                if (row.isValid()) {
                    run(command, row);
                }
            });
        }
    }
    // Finder rings the row a context menu belongs to while the menu is open.
    m_menuRow = index;
    viewport()->update();
    menu.exec(event->globalPos());
    m_menuRow = QPersistentModelIndex();
    viewport()->update();
}

void SidebarView::run(SidebarCommand command, const QModelIndex &index)
{
    switch (command) {
    case SidebarCommand::Open:
        m_model->activate(index);
        break;
    case SidebarCommand::OpenInNewTab:
        m_model->activate(index, OpenIn::NewTab);
        break;
    case SidebarCommand::GetInfo:
        KPropertiesDialog::showDialog(index.data(SidebarModel::UrlRole).toUrl(), window(), false);
        break;
    case SidebarCommand::Rename:
        edit(index);
        break;
    case SidebarCommand::Eject:
        m_model->eject(index);
        break;
    case SidebarCommand::Unmount:
        m_model->unmount(index);
        break;
    case SidebarCommand::RemoveFromSidebar:
        m_model->removeFromSidebar(index);
        break;
    case SidebarCommand::EmptyTrash:
        emptyTrash(window());
        break;
    }
}

SidebarPanel::SidebarPanel(PathContext context, QWidget *parent)
    : QWidget(parent)
    , m_header(new QWidget(this))
    , m_headerLayout(new QHBoxLayout(m_header))
    , m_view(new SidebarView(std::move(context), this))
{
    setFixedWidth(style::SIDEBAR_WIDTH);
    m_header->setFixedHeight(style::TOOLBAR_HEIGHT - style::SIDEBAR_INSET);
    m_headerLayout->setContentsMargins(12, 0, 10, 0);
    m_headerLayout->setSpacing(6);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 8);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    layout->addWidget(m_view, 1);
}

SidebarView *SidebarPanel::view() const
{
    return m_view;
}

QWidget *SidebarPanel::header() const
{
    return m_header;
}

QHBoxLayout *SidebarPanel::headerLayout() const
{
    return m_headerLayout;
}

void SidebarPanel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    // The window leaves this area unpainted (MainWindow::glassCutout), so this is the only glass here.
    style::paintGlass(painter, QRectF(rect()), style::SIDEBAR_RADIUS,
                      isActiveWindow() ? style::SIDEBAR_GLASS : style::SIDEBAR_GLASS_INACTIVE);
}

} // namespace fe
