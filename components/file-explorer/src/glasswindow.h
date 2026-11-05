// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The base class of File Explorer windows: frameless glass with a blurred background, a shadow, a rim, and resizing
// and moving by hand.

#pragma once

#include <QPainterPath>
#include <QPointer>
#include <QWidget>

/// A frameless, translucent top-level window drawn as one sheet of glass: KWin blurs what is behind
/// it and draws its shadow, and the window draws its own rounded body and rim. Its inner band of
/// RESIZE_BORDER pixels resizes it; registered drag surfaces move it.
class GlassWindow : public QWidget
{
    Q_OBJECT

public:
    explicit GlassWindow(QWidget *parent = nullptr);

    /// Lets presses on the empty parts of surface move the window; a double click zooms it.
    void addDragSurface(QWidget *surface);
    /// Maximizes the window, or restores it when it is maximized.
    void toggleMaximized();
    /// The radius of the window body in pixels; 0 when it is maximized or full screen.
    [[nodiscard]] qreal cornerRadius() const;

protected:
    /// A part of the window the window tint leaves out, for a child that paints its own glass there.
    [[nodiscard]] virtual QPainterPath glassCutout() const;

    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Asks KWin to blur behind the rounded body.
    void updateBlurRegion();
    /// Attaches the window shadow, or removes it when the window fills the screen.
    void updateShadow();

    QList<QPointer<QWidget>> m_dragSurfaces;
};
