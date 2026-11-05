// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The two rows at the bottom of a window: the status row with its toggles, and the path bar.

#pragma once

#include "core/paths.h"

#include <QWidget>

class QAbstractButton;

namespace fe {

/// The row under the files: toggles for hidden files and previews, Get Info, and the item count.
class StatusRow : public QWidget
{
    Q_OBJECT

public:
    explicit StatusRow(QWidget *parent = nullptr);
    /// Sets the text at the right end of the row (the item count and free space).
    void setText(const QString &text);
    /// Shows whether hidden files are shown, on the toggle and in its tooltip.
    void setHiddenShown(bool shown);
    /// Shows whether previews are shown, on the toggle and in its tooltip.
    void setPreviewsShown(bool shown);

Q_SIGNALS:
    /// The hidden files toggle was clicked; the row does not change its own state.
    void hiddenToggled();
    /// The previews toggle was clicked; the row does not change its own state.
    void previewsToggled();
    /// The Get Info button was clicked.
    void infoClicked();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QAbstractButton *m_hidden;
    QAbstractButton *m_previews;
    QString m_text;
};

/// The path bar along the bottom edge of the window: one segment per folder, each one a link.
class PathBar : public QWidget
{
    Q_OBJECT

public:
    explicit PathBar(QWidget *parent = nullptr);
    /// Replaces the segments. A path wider than the bar folds its middle segments into an ellipsis.
    void setCrumbs(const QList<Crumb> &crumbs);

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
    [[nodiscard]] int segmentAt(QPointF pos) const;

    QList<Crumb> m_crumbs;
    QList<Segment> m_segments;
    int m_hover = -1;
};

} // namespace fe
