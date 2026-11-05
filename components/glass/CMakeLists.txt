# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# org.aurora.glass: per-shape KWin blur for Plasma surfaces (the Control Center's floating modules), and the
# pointing hand over everything clickable in plasmashell (PointerHints).
cmake_minimum_required(VERSION 3.24)
project(glass LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)
# KWindowEffects, from KF6WindowSystem, sends the blur region to KWin.
find_package(KF6WindowSystem 6.0 REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)

# A plugin-only module: the plugin is the backing target, so one .so holds the types.
qt_add_qml_module(auroraglass
    URI org.aurora.glass
    VERSION 1.0
    PLUGIN_TARGET auroraglass
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/org/aurora/glass
    SOURCES src/blurbehind.h src/blurbehind.cpp src/pointerhints.h src/pointerhints.cpp
)
target_include_directories(auroraglass PUBLIC src)
target_link_libraries(auroraglass PRIVATE Qt6::Quick KF6::WindowSystem)
# It links only the system's Qt and KDE libraries, so it carries no RUNPATH (the build tree's would ship empty).
set_target_properties(auroraglass PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "")

set(GLASS_QML_DIR "$ENV{HOME}/.local/lib/qt6/qml/org/aurora/glass" CACHE PATH "Where the module is installed")
install(TARGETS auroraglass LIBRARY DESTINATION ${GLASS_QML_DIR})
install(FILES ${CMAKE_BINARY_DIR}/qml/org/aurora/glass/qmldir
              ${CMAKE_BINARY_DIR}/qml/org/aurora/glass/auroraglass.qmltypes
        DESTINATION ${GLASS_QML_DIR})
