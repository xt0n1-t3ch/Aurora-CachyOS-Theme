// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The sidebar: the model that regroups KDE's places as Favorites and Locations, the list view with its drag and drop
// and menus, and the glass panel around it.

#pragma once

#include "core/paths.h"
#include "core/places.h"

#include <QAbstractListModel>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QListView>
#include <QPersistentModelIndex>
#include <QWidget>

class KFilePlacesModel;
class QHBoxLayout;
class QMimeData;
class QTimer;
class QVariantAnimation;

namespace fe {

/// Where a place opens.
enum class OpenIn {
    CurrentTab,
    NewTab,
};

/// The Finder sidebar rows: section headers and places, built from KDE's places (the same list
/// Dolphin shows, so bookmarks stay in sync), regrouped as Favorites and Locations, then the color Tags. A folder
/// the user added unfolds into its subfolders, and a section folds away under its title.
class SidebarModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        UrlRole = Qt::UserRole + 1,
        /// A theme icon name, or a resource path starting with ":/" for the app's own symbols.
        IconNameRole,
        /// The row is a section title (Favorites, Locations), not a place.
        IsHeaderRole,
        /// A drive: its row draws a capacity bar under its name.
        IsDriveRole,
        /// The drive's used share from 0 to 1, or nothing while its size is unknown.
        CapacityRole,
        /// The drive's fill level (fe::Capacity::Level) as an int.
        CapacityLevelRole,
        /// How deep a subfolder row sits under its favorite: 0 for the sidebar's own rows.
        DepthRole,
        /// The row can unfold into subfolders (and draws a chevron).
        ExpandableRole,
        /// The row shows its subfolders, or for a section title, its section is open.
        ExpandedRole,
        /// For a tag row, the tag's color; nothing for any other row.
        TagColorRole,
    };

    /// context supplies the home folder and the user name that Home shows.
    explicit SidebarModel(PathContext context, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;
    /// Renames a drive (the name is the sidebar's own; the disk keeps its label) or a folder the user added.
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    [[nodiscard]] QModelIndex indexForUrl(const QUrl &url) const;
    /// Opens the place at index: placeReady follows, after mounting it first if it is an unmounted drive.
    void activate(const QModelIndex &index, OpenIn where = OpenIn::CurrentTab);
    /// What the row at index is and can do, for its context menu.
    [[nodiscard]] SidebarPlace placeAt(const QModelIndex &index) const;
    /// Puts a folder at the end of Favorites. KDE's places hold the sidebar, so Dolphin shows it too.
    void addFolder(const QUrl &url, const QString &title);
    /// Takes a folder the user added out of the sidebar again.
    void removeFromSidebar(const QModelIndex &index);
    /// Opens a disc's tray, or unmounts a drive so it can be pulled out.
    void eject(const QModelIndex &index);
    /// Unmounts the drive at index.
    void unmount(const QModelIndex &index);
    /// Whether files dropped on the row at index can go into its place.
    [[nodiscard]] bool acceptsFiles(const QModelIndex &index) const;
    /// Whether the row at index is one of Favorites.
    [[nodiscard]] bool inFavorites(const QModelIndex &index) const;
    /// Puts folders into Favorites next to the row at index: below it, or above it.
    void insertFolders(const QList<QUrl> &folders, const QModelIndex &index, bool below);
    /// Whether the rows at a and b are places of the same section (Favorites or Locations).
    [[nodiscard]] bool sameSection(const QModelIndex &a, const QModelIndex &b) const;
    /// Moves the row at from right above or right below the row at to, in their section; the order is saved
    /// ([Sidebar Order] in file-explorerrc) and outlives the places changing.
    void moveRow(const QModelIndex &from, const QModelIndex &to, bool below);
    /// Unfolds a folder row into its subfolders, or folds them away; remembered in file-explorerrc.
    void toggleExpanded(const QModelIndex &index);
    /// Opens or folds the section under a section title; remembered in file-explorerrc.
    void toggleSection(const QModelIndex &index);

Q_SIGNALS:
    /// A place is ready to open, after it was mounted if it needed to be.
    void placeReady(const QUrl &url, fe::OpenIn where);
    /// The system disk has a new name; the path bar starts with it.
    void systemNameChanged(const QString &name);

private:
    struct Row {
        QString title;
        QString iconName;
        QUrl url;
        bool header = false;
        QPersistentModelIndex place;
        bool addedByUser = false;
        /// For a drive, where its own name is kept (fe::Drive::key); empty for any other row.
        QString driveKey;
        /// For a drive, its name when the user has given it none.
        QString defaultTitle;
        int depth = 0;
        bool expandable = false;
        /// Shows its subfolders; for a section title, its section is open.
        bool expanded = false;
        /// For a tag row, the tag's name; empty for any other row.
        QString tag = {};
    };
    struct FreeSpace {
        quint64 size = 0;
        quint64 available = 0;
    };

    [[nodiscard]] const Row *rowAt(const QModelIndex &index) const;
    /// What names a row in the saved order: a drive's own key, else its address.
    [[nodiscard]] static QString orderKey(const Row &row);
    /// The keys of the section's rows in the order on screen: Favorites, or Locations.
    [[nodiscard]] QStringList sectionKeys(bool favorites) const;
    [[nodiscard]] QModelIndex placeIndexFor(const QUrl &url) const;
    void rebuild();
    /// Adds a section title and, unless the section is folded, its rows; tree unfolds the folders the user added.
    void appendSection(const QString &title, const QList<Row> &rows, bool tree);
    /// The place of a row among the sidebar's own rows of its section, leaving out subfolder rows.
    [[nodiscard]] qsizetype topLevelPosition(int row) const;
    /// Asks KIO how full each mounted drive is; rows update as the answers come.
    void refreshFreeSpace();

    PathContext m_context;
    KFilePlacesModel *m_places;
    QList<Row> m_rows;
    QPersistentModelIndex m_pendingSetup;
    OpenIn m_pendingTarget = OpenIn::CurrentTab;
    /// The row of the Locations header, which ends Favorites, and of the Tags header, which ends Locations.
    int m_locationsHeader = 0;
    int m_tagsHeader = 0;
    /// The folders shown unfolded, and the sections folded away ([Sidebar Tree] in file-explorerrc).
    QSet<QString> m_expanded;
    QSet<QString> m_foldedSections;
    /// The user's names for drives, by fe::Drive::key ([Drive Names] in file-explorerrc).
    QMap<QString, QString> m_names;
    QString m_hostName;
    QHash<QString, FreeSpace> m_freeSpace;
    QSet<QString> m_freeSpaceAsked;
    QTimer *m_freeSpaceTimer;
};

/// The list inside the sidebar: transparent, painted by SidebarDelegate, with Finder's row menus.
class SidebarView : public QListView
{
    Q_OBJECT

public:
    explicit SidebarView(PathContext context, QWidget *parent = nullptr);
    /// Highlights the row that leads to url, or none when no row does. The choice survives changes to the places.
    void setCurrentFolder(const QUrl &url);
    /// Whether a row of the sidebar leads to url.
    [[nodiscard]] bool showsFolder(const QUrl &url) const;
    /// Puts a folder at the end of Favorites under title.
    void addFolder(const QUrl &url, const QString &title);
    /// The row whose context menu is open; Finder rings it meanwhile.
    [[nodiscard]] QModelIndex menuRow() const;
    /// The row a drag would drop files into; it lights up meanwhile.
    [[nodiscard]] QModelIndex dropTargetRow() const;
    /// How far the row's hover has eased in, from 0 to 1; drives thicken their capacity bar with it.
    [[nodiscard]] qreal hoverProgress(const QModelIndex &index) const;

Q_SIGNALS:
    /// A place was chosen and should open in the current tab.
    void placeActivated(const QUrl &url);
    /// A place was chosen and should open in a new tab.
    void newTabRequested(const QUrl &url);
    /// The system disk has a new name.
    void systemNameChanged(const QString &name);

protected:
    [[nodiscard]] QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex &index,
                                                                       const QEvent *event = nullptr) const override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    bool viewportEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    /// Carries out a command of the row menu on the row at index.
    void run(SidebarCommand command, const QModelIndex &index);
    /// Remembers what a drag is over, to light the row up or draw the insertion line.
    void setDropTarget(const QModelIndex &index, SidebarDrop drop);
    /// Follows the pointer: the hand over a place, the hover easing in on the row under it.
    void setHoverRow(const QModelIndex &index);
    /// Drags the row pressed last, so it can move within its section.
    void dragPressedRow();
    /// The row a drag carries when it is a row of this sidebar being moved, or none.
    [[nodiscard]] QModelIndex movingRow(const QMimeData *mime) const;

    SidebarModel *m_model;
    QUrl m_currentFolder;
    QPersistentModelIndex m_menuRow;
    QPersistentModelIndex m_dropRow;
    SidebarDrop m_drop = SidebarDrop::Nowhere;
    QPersistentModelIndex m_hoverRow;
    /// The place row the left button went down on, and where; a drag of it moves the row.
    QPersistentModelIndex m_pressRow;
    QPoint m_pressPos;
    qreal m_hoverProgress = 0;
    QVariantAnimation *m_hoverAnimation;
};

/// The floating glass panel on the left: a header row for the window buttons, then the places.
class SidebarPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SidebarPanel(PathContext context, QWidget *parent = nullptr);
    [[nodiscard]] SidebarView *view() const;
    /// The header row; the window buttons and the sidebar buttons live here.
    [[nodiscard]] QWidget *header() const;
    [[nodiscard]] QHBoxLayout *headerLayout() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QWidget *m_header;
    QHBoxLayout *m_headerLayout;
    SidebarView *m_view;
};

} // namespace fe
