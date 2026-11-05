# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Quick Menu: Plasma's own KRunner, built from plasma-workspace v6.8.2 with its window drawn as macOS
# Tahoe's Spotlight (qml/RunCommand.qml and the QuickMenu*.qml beside it, a frameless glass window in view.cpp). It
# keeps the D-Bus name org.kde.krunner, its runners and its settings; the user unit drop-in that starts it instead
# of /usr/bin/krunner is the only switch, so removing the drop-in brings the stock KRunner back.
cmake_minimum_required(VERSION 3.24)
project(quick-menu VERSION 6.8.2 LANGUAGES CXX)

# Minimum Qt and KDE Frameworks versions, and the Plasma version of the vendored sources.
set(QT_MIN_VERSION "6.9.0")
set(KF6_MIN_VERSION "6.18.0")
set(PROJECT_DEP_VERSION "6.8.2")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQmlModule)
include(ECMGenerateDBusServiceFile)
include(ECMConfiguredInstall)
include(CMakePackageConfigHelpers)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS DBus Gui Qml Quick Widgets ShaderTools)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Config Crash DBusAddons I18n KIO Runner WindowSystem)
find_package(Plasma ${PROJECT_DEP_VERSION} REQUIRED)
find_package(PlasmaQuick ${PROJECT_DEP_VERSION} REQUIRED)
find_package(PlasmaActivities ${PROJECT_DEP_VERSION} REQUIRED)
find_package(LibKWorkspace ${PROJECT_DEP_VERSION} REQUIRED)
find_package(LayerShellQt ${PROJECT_DEP_VERSION} REQUIRED)

set(PLASMA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/vendor" CACHE PATH "plasma-workspace sources to build KRunner from")
add_subdirectory(${PLASMA_SOURCES}/krunner ${CMAKE_CURRENT_BINARY_DIR}/krunner)
