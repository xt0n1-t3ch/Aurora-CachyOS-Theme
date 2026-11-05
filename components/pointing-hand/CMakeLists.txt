# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# aurora-pointer: a KWin plugin that shows the pointing hand over window decoration buttons. KWin 6.8.2 always
# shows the arrow over a decoration outside its resize edges (window.cpp:2407-2455) and KDecoration3 has no cursor
# API, so the plugin follows the pointer and swaps the cursor source through KWin's exported cursor API. No KWin
# patch: KWin loads it only when it was built for the same KWin (the plugin IID carries the version, plugin.h:19).
cmake_minimum_required(VERSION 3.24)
project(pointing-hand LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(ECM REQUIRED NO_MODULE)
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets Quick)
find_package(KWin REQUIRED)
find_package(KDecoration3 REQUIRED)

add_library(aurora-pointer MODULE src/pointinghand.cpp)
set_target_properties(aurora-pointer PROPERTIES PREFIX "")
target_link_libraries(aurora-pointer PRIVATE KWin::kwin KDecoration3::KDecoration Qt6::Core Qt6::Gui)
install(TARGETS aurora-pointer DESTINATION $ENV{HOME}/.local/lib/qt6/plugins/kwin/plugins)
