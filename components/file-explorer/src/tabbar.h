// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The tab bar of a window: one glass track with a pill per tab and a button for a new tab.

#pragma once

#include "core/motion.h"

#include <QStringList>
#include <QWidget>

namespace fe {

/// Finder's tab bar: one glass track across the content column, every tab an equal share of it and
/// the current one raised, with a round "+" at the end. It hides itself while there is one tab.
class PillTabBar : public QWidget
{
    Q_OBJECT

public:
    explicit PillTabBar(QWidget *parent = nullptr);

    /// Inserts a tab at index (clamped to the tab count). The first tab becomes the current one.
    void insertTab(int index, const QString &title);
    /// Removes the tab at index; an index out of range does nothing.
    void removeTab(int index);
    void setTabTitle(int index, const QString &title);
    /// Raises a tab without sending tabClicked. An index out of range does nothing.
    void setCurrentIndex(int index);
    /// The raised tab, or -1 when there are no tabs.
    [[nodiscard]] int currentIndex() const;
    [[nodiscard]] int count() const;

Q_SIGNALS:
    /// A tab was pressed (not its close button).
    void tabClicked(int index);
    /// The close button of a tab was clicked, or the tab was pressed with the middle button.
    void tabCloseRequested(int index);
    /// The "+" button was clicked.
    void newTabClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class Part {
        None,
        Tab,
        Close,
        Plus,
    };
    struct Hit {
        Part part = Part::None;
        int index = -1;
    };

    [[nodiscard]] QRectF trackRect() const;
    [[nodiscard]] QRectF tabRect(int index) const;
    [[nodiscard]] QRectF closeRect(int index) const;
    [[nodiscard]] QRectF plusRect() const;
    [[nodiscard]] Hit hitAt(QPointF pos) const;
    void updateVisibility();

    QStringList m_titles;
    int m_current = -1;
    Hit m_hover;
    motion::HoverFade *m_fade;
    Hit m_pressed;
};

} // namespace fe
