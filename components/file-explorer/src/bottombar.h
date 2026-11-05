// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The bar at the bottom of a window: the path of the folder or the selected item, and the item count.

#pragma once

#include "core/paths.h"

#include <QWidget>

namespace fe {

/// The path bar under the files: one segment per folder, each one a link, and the item count and free space at its
/// right end.
class PathBar : public QWidget
{
    Q_OBJECT

public:
    explicit PathBar(QWidget *parent = nullptr);
    /// Replaces the segments. A path wider than the bar folds its middle segments into an ellipsis. lastIconName,
    /// when given, is the icon of the last segment (a selected file's type).
    void setCrumbs(const QList<Crumb> &crumbs, const QString &lastIconName = {});
    /// Sets the text at the right end (the item count and free space).
    void setStatusText(const QString &text);

Q_SIGNALS:
    /// A segment was clicked.
    void crumbClicked(const QUrl &url);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Segment {
        Crumb crumb;
        QString iconName;
        QRectF rect;
        bool ellipsis = false;
    };

    void layoutSegments();
    /// The width the status text takes at the right end, with its gap.
    [[nodiscard]] qreal statusWidth() const;
    [[nodiscard]] int segmentAt(QPointF pos) const;

    QList<Crumb> m_crumbs;
    QString m_lastIconName;
    QString m_status;
    QList<Segment> m_segments;
    int m_hover = -1;
};

} // namespace fe
