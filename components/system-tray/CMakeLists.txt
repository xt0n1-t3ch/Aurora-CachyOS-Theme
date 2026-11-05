# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Aurora system tray: Plasma's own system tray applet, built from plasma-workspace v6.8.2 with
# its C++ untouched and only its QML changed (tray icons can be dragged). It keeps the plugin id
# org.kde.plasma.systemtray, so Plasma loads it ahead of the system copy through QT_PLUGIN_PATH and
# every panel keeps its tray and settings; deleting the .so brings the stock tray back.
cmake_minimum_required(VERSION 3.24)
project(system-tray VERSION 6.8.2 LANGUAGES CXX)

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

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS DBus Gui GuiPrivate Qml Quick Widgets)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Config IconThemes I18n ItemModels JobWidgets KIO Service StatusNotifierItem WindowSystem XmlGui)
find_package(Plasma ${PROJECT_DEP_VERSION} REQUIRED)
find_package(PlasmaQuick ${PROJECT_DEP_VERSION} REQUIRED)

# The plasma-workspace sources of the installed Plasma, with system-tray.patch applied to the tray's QML.
set(PLASMA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/plasma-workspace" CACHE PATH "plasma-workspace sources to build the tray from")
set(plasma-workspace_SOURCE_DIR ${PLASMA_SOURCES})
# plasma-workspace generates this from its top level; the tray only asks whether X11 is there.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/config-X11.h "#define HAVE_X11 1\n")
include_directories(${CMAKE_CURRENT_BINARY_DIR}
                    ${plasma-workspace_SOURCE_DIR}/libdbusmenuqt
                    ${CMAKE_CURRENT_BINARY_DIR}/vendor/plasma-workspace/libdbusmenuqt)

add_subdirectory(${PLASMA_SOURCES}/libdbusmenuqt ${CMAKE_CURRENT_BINARY_DIR}/vendor/plasma-workspace/libdbusmenuqt)
add_subdirectory(${PLASMA_SOURCES}/applets/systemtray ${CMAKE_CURRENT_BINARY_DIR}/vendor/plasma-workspace/applets/systemtray)
