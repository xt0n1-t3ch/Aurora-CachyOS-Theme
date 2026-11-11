// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The bar at the bottom of a window: the path of the folder or the selected item, and the item count.

#pragma once

#include "core/paths.h"

#include <QWidget>

namespace fe {

class CrumbStrip;

/// The path bar under the files: one segment per folder (fe::CrumbStrip: click, chevron, drag and drop), and the item
/// count and free space at its right end.
class PathBar : public QWidget
{
    Q_OBJECT

public:
    explicit PathBar(QWidget *parent = nullptr);
    /// Replaces the segments. A path wider than the bar folds its middle segments into an ellipsis. lastIconName,
    /// when given, is the icon of the last segment, and lastIsFolder is false when that segment is the selected file.
    void setCrumbs(const QList<Crumb> &crumbs, const QString &lastIconName = {}, bool lastIsFolder = true);
    /// Sets the text at the right end (the item count and free space).
    void setStatusText(const QString &text);
    /// The segments, whose signals say what the user chose.
    [[nodiscard]] CrumbStrip *crumbs() const;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    /// The width the status text takes at the right end, with its gap.
    [[nodiscard]] qreal statusWidth() const;
    void placeCrumbs();

    CrumbStrip *m_crumbs;
    QString m_status;
};

} // namespace fe
