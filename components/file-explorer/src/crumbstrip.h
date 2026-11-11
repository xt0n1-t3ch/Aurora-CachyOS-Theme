// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The clickable path segments shared by the address field and the path bar: their layout, painting, menus, drags out
// and drops in. The bar that shows them forwards its events here; the rules live in core/crumbs.

#pragma once

#include "core/crumbs.h"
#include "core/paths.h"

#include <QObject>
#include <QPointF>
#include <QRectF>

#include <optional>

class QContextMenuEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMouseEvent;
class QPainter;
class QTimer;
class QWidget;

namespace fe {

/// Where an activated segment opens.
enum class CrumbOpen {
    Here,
    NewTab,
};

/// A row of path segments drawn inside a host widget. The host calls paint() from its paint event and passes its
/// mouse, context menu and drag events to the handlers below, which return true when the strip used the event. A
/// click opens a segment, a middle click or Ctrl+click opens it in a new tab, the chevron after a segment lists the
/// folders in it, the ellipsis lists the folded segments, a drag takes a segment out as its URL, and files dropped on
/// a segment move, copy or link there through KIO (or get a shelf's tag). Holding a drag over a segment opens it.
/// Lives on the GUI thread; the host owns it.
class CrumbStrip : public QObject
{
    Q_OBJECT

public:
    /// host draws the strip and parents its menus. showIcons puts each segment's icon before its name. restCursor is
    /// the host's cursor away from the segments; none unsets it.
    CrumbStrip(QWidget *host, bool showIcons, std::optional<Qt::CursorShape> restCursor);

    /// Replaces the segments. lastIconName, when given, is the icon of the last segment; lastIsFolder is false when the
    /// last segment is the selected file.
    void setCrumbs(const QList<Crumb> &crumbs, const QString &lastIconName = {}, bool lastIsFolder = true);
    /// The folder of the tag shelves (fe::tagging::shelfRoot), whose shelves tag what is dropped on them.
    void setTagsPath(const QString &tagsPath);
    /// The part of the host the strip lays itself out in, in the host's coordinates.
    void setArea(const QRectF &area);
    [[nodiscard]] bool isEmpty() const;

    void paint(QPainter &painter) const;
    /// The tooltip for a point: the whole path of the segment under it, or an empty string away from the segments.
    [[nodiscard]] QString toolTipAt(QPointF pos) const;
    /// Whether a segment, its chevron or the ellipsis is at pos.
    [[nodiscard]] bool isOverCrumb(QPointF pos) const;

    bool mousePress(QMouseEvent *event);
    bool mouseMove(QMouseEvent *event);
    bool mouseRelease(QMouseEvent *event);
    void leave();
    bool contextMenu(QContextMenuEvent *event);
    bool dragEnter(QDragEnterEvent *event);
    bool dragMove(QDragMoveEvent *event);
    void dragLeave();
    bool drop(QDropEvent *event);

Q_SIGNALS:
    /// A segment was clicked, chosen in a menu, or held under a drag; url is where it leads.
    void crumbActivated(const QUrl &url, fe::CrumbOpen how);
    /// A segment was right-clicked at globalPos.
    void crumbMenuRequested(const QUrl &url, const QPoint &globalPos);

private:
    void relayout();
    [[nodiscard]] CrumbHit hitAt(QPointF pos) const;
    /// The segment a slot shows, or nullptr for the ellipsis.
    [[nodiscard]] const Crumb *crumbOf(qsizetype slot) const;
    [[nodiscard]] bool isFolderSlot(qsizetype slot) const;
    void setHover(CrumbHit hover);
    void setDropSlot(qsizetype slot);
    /// Lists the folders inside the segment at slot under its chevron; the next segment of the path is checked.
    void showFolderMenu(qsizetype slot);
    /// Lists the folded segments under the ellipsis at slot.
    void showFoldedMenu(qsizetype slot);
    void startDrag(qsizetype slot);
    /// What a drop of event's URLs on the segment at slot does, read-only folders refused.
    [[nodiscard]] CrumbDrop dropAt(qsizetype slot, const QDropEvent *event) const;

    QWidget *m_host;
    bool m_showIcons;
    std::optional<Qt::CursorShape> m_restCursor;
    QList<Crumb> m_crumbs;
    QString m_lastIconName;
    bool m_lastIsFolder = true;
    QString m_tagsPath;
    QRectF m_area;
    CrumbLayout m_layout;
    CrumbHit m_hover;
    /// The slot a press started on, for a click or a drag out; -1 for none.
    qsizetype m_pressSlot = -1;
    QPointF m_pressPos;
    /// The slot under a drag that would take the drop, drawn with the accent; -1 for none.
    qsizetype m_dropSlot = -1;
    /// Opens the segment held under a drag, as Finder's spring-loaded folders do.
    QTimer *m_springTimer;
};

} // namespace fe
