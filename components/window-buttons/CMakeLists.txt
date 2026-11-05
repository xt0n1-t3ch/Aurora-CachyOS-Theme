# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Builds the Aurora window decoration (a KDecoration3 plugin for KWin), its preview tool and its tests.
cmake_minimum_required(VERSION 3.24)
project(window-buttons VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(GNUInstallDirs)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Test)
find_package(KDecoration3 REQUIRED)
find_package(KF6CoreAddons REQUIRED)

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Werror)

# The title bar geometry, with no KWin types, so the tests stay small and fast.
add_library(titlelayout_core STATIC src/titlelayout.cpp)
set_target_properties(titlelayout_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(titlelayout_core PUBLIC src)
target_link_libraries(titlelayout_core PUBLIC Qt6::Core)

# The decoration itself, shared by the KWin plugin and the preview tool.
add_library(decoration_core OBJECT src/decoration.cpp src/button.cpp src/glass.cpp)
set_target_properties(decoration_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(decoration_core PUBLIC titlelayout_core Qt6::Gui KDecoration3::KDecoration KF6::CoreAddons)

add_library(auroradecoration MODULE)
set_target_properties(auroradecoration PROPERTIES OUTPUT_NAME org.aurora.decoration PREFIX "")
target_link_libraries(auroradecoration PRIVATE decoration_core)

# Draws every state of the decoration through kdecoration's test bridge; not installed.
add_executable(deco-preview tools/preview.cpp tools/previewbridge.cpp)
# The preview bridge derives from kdecoration's private classes, which its private library exports.
target_link_libraries(deco-preview PRIVATE decoration_core /usr/lib/libkdecorations3private.so)

enable_testing()
add_executable(decoration_tests tests/test_titlelayout.cpp)
target_link_libraries(decoration_tests PRIVATE titlelayout_core Qt6::Test)
add_test(NAME decoration_tests COMMAND decoration_tests)

install(TARGETS auroradecoration DESTINATION ${CMAKE_INSTALL_LIBDIR}/qt6/plugins/org.kde.kdecoration3)
