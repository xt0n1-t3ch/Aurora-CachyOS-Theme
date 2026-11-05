// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// A stand-in for KWin, so the decoration can be drawn and driven outside a compositor. Adapted from
// kdecoration's own test mocks (autotests/mockbridge.h, mockwindow.h, mocksettings.h, v6.8.2).

#pragma once

#include <KDecoration3/Private/DecoratedWindowPrivate>
#include <KDecoration3/Private/DecorationBridge>
#include <KDecoration3/Private/DecorationSettingsPrivate>

#include <QPalette>
#include <QString>

namespace preview {

// The window the next decoration is created for.
struct WindowState {
    QString caption;
    QPalette palette;
    qreal width = 760;
    qreal height = 300;
    bool active = true;
    bool maximized = false;
    bool minimizeable = true;
    bool maximizeable = true;
    bool closeable = true;
};

// The window as the decoration sees it. It answers from a WindowState, gives fixed answers for what a
// preview does not need, and ignores every request the decoration makes.
class Window : public KDecoration3::DecoratedWindowPrivateV4
{
public:
    Window(KDecoration3::DecoratedWindow *window, KDecoration3::Decoration *decoration, WindowState state);

    bool isActive() const override;
    QString caption() const override;
    bool isOnAllDesktops() const override;
    bool isShaded() const override;
    QIcon icon() const override;
    bool isMaximized() const override;
    bool isMaximizedHorizontally() const override;
    bool isMaximizedVertically() const override;
    bool isKeepAbove() const override;
    bool isKeepBelow() const override;
    bool isExcludedFromCapture() const override;
    bool isCloseable() const override;
    bool isMaximizeable() const override;
    bool isMinimizeable() const override;
    bool providesContextHelp() const override;
    bool isModal() const override;
    bool isShadeable() const override;
    bool isMoveable() const override;
    bool isResizeable() const override;
    qreal width() const override;
    qreal height() const override;
    QSizeF size() const override;
    QPalette palette() const override;
    Qt::Edges adjacentScreenEdges() const override;
    qreal scale() const override;
    qreal nextScale() const override;
    QString windowClass() const override;
    bool hasApplicationMenu() const override;
    bool isApplicationMenuActive() const override;
    QString applicationMenuServiceName() const override;
    QString applicationMenuObjectPath() const override;
    void requestShowToolTip(const QString &text) override;
    void requestHideToolTip() override;
    void requestClose() override;
    void requestToggleMaximization(Qt::MouseButtons buttons) override;
    void requestMinimize() override;
    void requestContextHelp() override;
    void requestToggleOnAllDesktops() override;
    void requestToggleShade() override;
    void requestToggleKeepAbove() override;
    void requestToggleKeepBelow() override;
    void requestToggleExcludeFromCapture() override;
    void requestShowWindowMenu(const QRect &rect) override;
    void showApplicationMenu(int actionId) override;
    void requestShowApplicationMenu(const QRect &rect, int actionId) override;
    void popup(const KDecoration3::Positioner &positioner, QMenu *menu) override;

private:
    WindowState m_state;
};

// The decoration settings of the sandbox desktop: buttons at the right in Windows order, no border and an alpha
// channel.
class Settings : public KDecoration3::DecorationSettingsPrivateV2
{
public:
    explicit Settings(KDecoration3::DecorationSettings *parent);

    bool isOnAllDesktopsAvailable() const override;
    bool isAlphaChannelSupported() const override;
    bool isCloseOnDoubleClickOnMenu() const override;
    bool isAlwaysShowExcludeFromCapture() const override;
    QList<KDecoration3::DecorationButtonType> decorationButtonsLeft() const override;
    QList<KDecoration3::DecorationButtonType> decorationButtonsRight() const override;
    KDecoration3::BorderSize borderSize() const override;
    QFont font() const override;
};

// The bridge a decoration is created through. It hands out Window and Settings objects; the Window copies
// `nextWindow` when the decoration asks for its client.
class Bridge : public KDecoration3::DecorationBridge
{
    Q_OBJECT

public:
    WindowState nextWindow;

    std::unique_ptr<KDecoration3::DecoratedWindowPrivate> createClient(KDecoration3::DecoratedWindow *window,
                                                                        KDecoration3::Decoration *decoration) override;
    std::unique_ptr<KDecoration3::DecorationSettingsPrivate> settings(KDecoration3::DecorationSettings *parent) override;
};

} // namespace preview
