# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Builds the theme's Energy page for Info Center: what the CPU package (RAPL) and the NVIDIA GPU (NVML) draw right
# now, their history and the power profile. It keeps the stock page's plugin id, kcm_energyinfo, and installs under
# ~/.local/lib/qt6/plugins, which the session lists before /usr/lib/qt6/plugins, so it replaces the stock page that
# only knows batteries (KPluginMetaData::findPlugins keeps the first plugin of an id, kpluginmetadata.cpp:297).
cmake_minimum_required(VERSION 3.24)
project(energy-page LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Qml Quick DBus Test)
find_package(KF6 REQUIRED COMPONENTS KCMUtils CoreAddons)
include(KDEInstallDirs)
# A real plugin (kcoreaddons_add_plugin builds a static one without shared libraries) in build/bin/<namespace>.
set(BUILD_SHARED_LIBS ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(power_core STATIC src/power.cpp src/power.h)
target_include_directories(power_core PUBLIC src)
target_link_libraries(power_core PUBLIC Qt6::Core)

kcmutils_add_qml_kcm(kcm_energyinfo INSTALL_NAMESPACE "plasma/kcms/kinfocenter" SOURCES src/kcm.cpp src/powermonitor.cpp src/powermonitor.h)
set_target_properties(kcm_energyinfo PROPERTIES PREFIX "")
# CMAKE_DL_LIBS is for dlopen, which loads NVML at run time.
target_link_libraries(kcm_energyinfo PRIVATE power_core Qt6::Quick Qt6::DBus KF6::KCMUtilsQuick KF6::CoreAddons ${CMAKE_DL_LIBS})

add_executable(test_power tests/test_power.cpp)
target_link_libraries(test_power PRIVATE power_core Qt6::Test)
