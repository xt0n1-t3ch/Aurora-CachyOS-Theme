# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Builds fake-tray, the sandbox's stand-in for a tray app (see main.cpp). Scenarios run it from build/ in this folder.
cmake_minimum_required(VERSION 3.24)
project(fake-tray LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
add_executable(fake-tray main.cpp)
target_link_libraries(fake-tray PRIVATE Qt6::Widgets)
