// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The minimize, maximize and close buttons of a glass window.

#pragma once

#include "core/motion.h"

#include <QWidget>

/// The window buttons in Windows order (minimize, maximize, close), in one Liquid Glass capsule with
/// the height and segment width of the toolbar's capsules. Close turns red under the pointer.
class WindowControls : public QWidget
{
    Q_OBJECT

public:
    explicit WindowControls(QWidget *parent = nullptr);
    /// Shows the restore glyph instead of the maximize glyph.
    void setMaximized(bool maximized);
    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    /// Each signal is sent when a press and its release both land on that button.
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    [[nodiscard]] QRectF segmentRect(int index) const;
    [[nodiscard]] int segmentAt(QPointF pos) const;
    void setHover(int index);

    bool m_maximized = false;
    int m_hover = -1;
    fe::motion::HoverFade *m_fade;
    int m_pressed = -1;
};
