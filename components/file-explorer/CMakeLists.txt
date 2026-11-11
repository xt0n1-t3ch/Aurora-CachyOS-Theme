# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Builds the File Explorer: the core library, the app, the screenshot driver and the core tests.

cmake_minimum_required(VERSION 3.24)
project(file-explorer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core DBus Gui Widgets Test)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6Parts REQUIRED)
find_package(KF6KIO REQUIRED)
find_package(KF6Solid REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6WindowSystem REQUIRED)
find_package(KF6XmlGui REQUIRED)
find_package(KF6FileMetaData REQUIRED)
find_package(KF6Purpose REQUIRED)

# -Wconversion stays off on purpose: Qt's int, qsizetype and qreal geometry APIs make it fire on
# almost every layout call, which would bury the warnings that matter.
add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Werror)

# Logic with no widgets, so the tests stay small and fast.
add_library(fe_core STATIC
    src/core/history.cpp
    src/core/paths.cpp
    src/core/crumbs.cpp
    src/core/edges.cpp
    src/core/format.cpp
    src/core/places.cpp
    src/core/reveal.cpp
    src/core/chooser.cpp
    src/core/drives.cpp
    src/core/motion.cpp
    src/core/address.cpp
    src/core/tags.cpp
    src/core/tree.cpp
    src/core/quicklook.cpp
    src/core/batchrename.cpp
    src/core/palette.cpp
    src/core/inspector.cpp)
target_include_directories(fe_core PUBLIC src)
target_link_libraries(fe_core PUBLIC Qt6::Core Qt6::Gui)

# Everything but main(): shared by the app and the screenshot driver.
add_library(fe_app OBJECT
    src/application.cpp
    src/style.cpp
    src/finderstyle.cpp
    src/shadow.cpp
    src/glasswindow.cpp
    src/windowcontrols.cpp
    src/sidebar.cpp
    src/toolbar.cpp
    src/tabbar.cpp
    src/browsertab.cpp
    src/crumbstrip.cpp
    src/crumbaccessible.cpp
    src/searchservice.cpp
    src/searchscopebar.cpp
    src/filterbar.cpp
    src/batchrenamesheet.cpp
    src/commandpalette.cpp
    src/glassfield.cpp
    src/inspectorpanel.cpp
    src/bottombar.cpp
    src/contextmenu.cpp
    src/mainwindow.cpp
    src/menubar.cpp
    src/filemanager1.cpp
    src/chooserbar.cpp
    src/portal.cpp
    src/addressfield.cpp
    src/actionbar.cpp
    src/tagging.cpp
    src/quicklook.cpp
    src/tabpage.cpp
    src/windowmenus.cpp
    resources/icons.qrc)
target_link_libraries(fe_app PUBLIC
    fe_core
    Qt6::DBus
    Qt6::Widgets
    KF6::CoreAddons
    KF6::Parts
    KF6::KIOCore
    KF6::KIOGui
    KF6::KIOWidgets
    KF6::KIOFileWidgets
    KF6::Solid
    KF6::ConfigCore
    KF6::WindowSystem
    KF6::XmlGui
    KF6::FileMetaData
    KF6::PurposeWidgets)

# The color tag dots on file icons, for File Explorer's and Dolphin's views (KIO loads kf6/overlayicon plugins).
add_library(aurora-tags MODULE src/plugins/tagoverlay.cpp)
target_link_libraries(aurora-tags PRIVATE fe_core KF6::KIOCore KF6::FileMetaData)
set_target_properties(aurora-tags PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf6/overlayicon")
# A module links fe_core's code into a shared object.
set_target_properties(fe_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The KIO worker of aurora-search: URLs, which lists the search helper's matches into Dolphin's view.
add_library(aurora-search-worker MODULE src/plugins/searchworker.cpp)
target_link_libraries(aurora-search-worker PRIVATE KF6::KIOCore)
set_target_properties(aurora-search-worker PROPERTIES PREFIX "" OUTPUT_NAME "aurora-search" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kf6/kio")

add_executable(file-explorer src/main.cpp)
target_link_libraries(file-explorer PRIVATE fe_app)

# Drives a window through its states and captures each one; used in the test sandbox, not installed.
add_executable(fe-shots tools/shots.cpp)
target_link_libraries(fe-shots PRIVATE fe_app)

enable_testing()
add_executable(fe_tests tests/test_core.cpp)
target_link_libraries(fe_tests PRIVATE fe_core Qt6::Test)
add_test(NAME fe_tests COMMAND fe_tests)
add_executable(fe_finder_tests tests/test_finder.cpp)
target_link_libraries(fe_finder_tests PRIVATE fe_core Qt6::Test)
add_test(NAME fe_finder_tests COMMAND fe_finder_tests)
# The widgets' behavior on the offscreen platform: clicks, drags and what assistive technology reads.
add_executable(fe_widget_tests tests/test_widgets.cpp)
target_link_libraries(fe_widget_tests PRIVATE fe_app Qt6::Test)
add_test(NAME fe_widget_tests COMMAND fe_widget_tests)
set_tests_properties(fe_widget_tests PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

install(TARGETS file-explorer)
