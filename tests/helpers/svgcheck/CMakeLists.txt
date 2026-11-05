# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Builds svgcheck, which renders SVG icons with QtSvg into one PNG (see main.cpp).
cmake_minimum_required(VERSION 3.24)
project(svgcheck LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 REQUIRED COMPONENTS Gui Svg)
add_executable(svgcheck main.cpp)
target_link_libraries(svgcheck PRIVATE Qt6::Gui Qt6::Svg)
