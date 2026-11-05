# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Aurora notifications: Plasma's own notifications applet, built from plasma-workspace v6.8.2 with its popups
# drawn as macOS banners (NotificationPopup.qml, delegates/DelegatePopup.qml, components/Banner*.qml, and an input
# region in notificationwindow.cpp). It keeps the plugin id org.kde.plasma.notifications, so Plasma loads it ahead of
# the system copy through QT_PLUGIN_PATH and the tray keeps its notifications and settings; deleting the .so brings
# the stock applet back.
cmake_minimum_required(VERSION 3.24)
project(notifications VERSION 6.8.2 LANGUAGES C CXX) # C: the generated Wayland protocol code

# Minimum Qt and KDE Frameworks versions, and the Plasma version of the vendored sources.
set(QT_MIN_VERSION "6.9.0")
set(KF6_MIN_VERSION "6.18.0")
set(PROJECT_DEP_VERSION "6.8.2")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)
include(ECMQmlModule)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS DBus Gui Qml Quick WaylandClient)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Config CoreAddons GlobalAccel I18n KIO Notifications Service WindowSystem)
find_package(Plasma ${PROJECT_DEP_VERSION} REQUIRED)
find_package(PlasmaQuick ${PROJECT_DEP_VERSION} REQUIRED)
find_package(LibNotificationManager ${PROJECT_DEP_VERSION} REQUIRED)
find_package(Wayland REQUIRED COMPONENTS Client)

# plasma-wayland-protocols is a build-time package that is not installed; its plasma-shell.xml is vendored.
set(PLASMA_WAYLAND_PROTOCOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/protocols")
# Used when the Wayland package reports no data directory; Arch installs wayland.xml here.
if(NOT Wayland_DATADIR)
    set(Wayland_DATADIR /usr/share/wayland)
endif()

set(PLASMA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/plasma-workspace" CACHE PATH "plasma-workspace sources to build the applet from")
add_subdirectory(${PLASMA_SOURCES}/applets/notifications ${CMAKE_CURRENT_BINARY_DIR}/vendor/plasma-workspace/applets/notifications)
