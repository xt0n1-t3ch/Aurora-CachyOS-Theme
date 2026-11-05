// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The menu bar names the active app the way macOS does: its name, not its window's title, and nothing at all when
// no window has the focus.
// Run: QT_QPA_PLATFORM=offscreen qmltestrunner -input tests/tst_apptitle.qml
import QtQuick
import QtTest
import "../package/contents/ui/apptitle.js" as AppTitle

TestCase {
    name: "AppTitle"

    function test_the_active_app_shows_its_name() {
        compare(AppTitle.label({ active: true, appName: "Konsole", title: "~ : bash" }), "Konsole")
    }

    function test_an_app_without_a_name_shows_its_window_title() {
        compare(AppTitle.label({ active: true, appName: "", title: "Steam" }), "Steam")
    }

    function test_spaces_around_the_name_go() {
        compare(AppTitle.label({ active: true, appName: "  Firefox  ", title: "" }), "Firefox")
    }

    function test_nothing_shows_when_no_window_has_the_focus() {
        compare(AppTitle.label({ active: false, appName: "Konsole", title: "~" }), "")
        compare(AppTitle.label({ active: true, appName: "", title: " " }), "")
    }
}
