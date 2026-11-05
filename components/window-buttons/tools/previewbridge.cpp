// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements the preview bridge: fixed answers for a window that is never shown.

#include "previewbridge.h"

#include <QFont>
#include <QIcon>

namespace preview {

Window::Window(KDecoration3::DecoratedWindow *window, KDecoration3::Decoration *decoration, WindowState state)
    : DecoratedWindowPrivateV4(window, decoration)
    , m_state(std::move(state))
{
}

bool Window::isActive() const { return m_state.active; }
QString Window::caption() const { return m_state.caption; }
bool Window::isOnAllDesktops() const { return false; }
bool Window::isShaded() const { return false; }
QIcon Window::icon() const { return {}; }
bool Window::isMaximized() const { return m_state.maximized; }
bool Window::isMaximizedHorizontally() const { return m_state.maximized; }
bool Window::isMaximizedVertically() const { return m_state.maximized; }
bool Window::isKeepAbove() const { return false; }
bool Window::isKeepBelow() const { return false; }
bool Window::isExcludedFromCapture() const { return false; }
bool Window::isCloseable() const { return m_state.closeable; }
bool Window::isMaximizeable() const { return m_state.maximizeable; }
bool Window::isMinimizeable() const { return m_state.minimizeable; }
bool Window::providesContextHelp() const { return false; }
bool Window::isModal() const { return false; }
bool Window::isShadeable() const { return false; }
bool Window::isMoveable() const { return true; }
bool Window::isResizeable() const { return true; }
qreal Window::width() const { return m_state.width; }
qreal Window::height() const { return m_state.height; }
QSizeF Window::size() const { return {m_state.width, m_state.height}; }
QPalette Window::palette() const { return m_state.palette; }
Qt::Edges Window::adjacentScreenEdges() const { return {}; }
qreal Window::scale() const { return 1; }
qreal Window::nextScale() const { return 1; }
QString Window::windowClass() const { return QStringLiteral("preview"); }
bool Window::hasApplicationMenu() const { return false; }
bool Window::isApplicationMenuActive() const { return false; }
QString Window::applicationMenuServiceName() const { return {}; }
QString Window::applicationMenuObjectPath() const { return {}; }
void Window::requestShowToolTip(const QString &) {}
void Window::requestHideToolTip() {}
void Window::requestClose() {}
void Window::requestToggleMaximization(Qt::MouseButtons) {}
void Window::requestMinimize() {}
void Window::requestContextHelp() {}
void Window::requestToggleOnAllDesktops() {}
void Window::requestToggleShade() {}
void Window::requestToggleKeepAbove() {}
void Window::requestToggleKeepBelow() {}
void Window::requestToggleExcludeFromCapture() {}
void Window::requestShowWindowMenu(const QRect &) {}
void Window::showApplicationMenu(int) {}
void Window::requestShowApplicationMenu(const QRect &, int) {}
void Window::popup(const KDecoration3::Positioner &, QMenu *) {}

Settings::Settings(KDecoration3::DecorationSettings *parent)
    : DecorationSettingsPrivateV2(parent)
{
}

bool Settings::isOnAllDesktopsAvailable() const { return false; }
bool Settings::isAlphaChannelSupported() const { return true; }
bool Settings::isCloseOnDoubleClickOnMenu() const { return false; }
bool Settings::isAlwaysShowExcludeFromCapture() const { return false; }

QList<KDecoration3::DecorationButtonType> Settings::decorationButtonsLeft() const
{
    return {};
}

QList<KDecoration3::DecorationButtonType> Settings::decorationButtonsRight() const
{
    // kwinrc ButtonsOnRight=IAX, the Windows order.
    return {KDecoration3::DecorationButtonType::Minimize, KDecoration3::DecorationButtonType::Maximize, KDecoration3::DecorationButtonType::Close};
}

KDecoration3::BorderSize Settings::borderSize() const
{
    return KDecoration3::BorderSize::None;
}

QFont Settings::font() const
{
    // kdeglobals [WM] activeFont in the sandbox: SF Pro Text, 10 pt, weight 600.
    QFont font(QStringLiteral("SF Pro Text"), 10);
    font.setWeight(QFont::DemiBold);
    return font;
}

std::unique_ptr<KDecoration3::DecoratedWindowPrivate> Bridge::createClient(KDecoration3::DecoratedWindow *window,
                                                                            KDecoration3::Decoration *decoration)
{
    return std::make_unique<Window>(window, decoration, nextWindow);
}

std::unique_ptr<KDecoration3::DecorationSettingsPrivate> Bridge::settings(KDecoration3::DecorationSettings *parent)
{
    return std::make_unique<Settings>(parent);
}

} // namespace preview
