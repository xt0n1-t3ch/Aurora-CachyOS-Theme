// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The menu bar clock reads like macOS's: "Wed Nov 4" then the time, or "mié 4 nov" in Spanish.
// Run: QT_QPA_PLATFORM=offscreen qmltestrunner -input tests/tst_clock.qml
import QtQuick
import QtTest
import "../package/contents/ui/clock.js" as Clock

TestCase {
    name: "Clock"

    readonly property var english: ({
        days: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"],
        months: ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"],
        am: "AM", pm: "PM", dayFirst: false, uses24h: false, padHours: false
    })
    // Qt's names under LC_TIME=es_PA.UTF-8: "mié", "nov", "a. m.", "p. m.".
    readonly property var spanish: ({
        days: ["dom", "lun", "mar", "mié", "jue", "vie", "sáb"],
        months: ["ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sept", "oct", "nov", "dic"],
        am: "a. m.", pm: "p. m.", dayFirst: true, uses24h: false, padHours: false
    })

    function test_an_english_morning_reads_like_macos() {
        const date = new Date(2026, 10, 4, 8, 1)
        compare(Clock.dateText(date, english), "Wed Nov 4")
        compare(Clock.timeText(date, english), "8:01 AM")
    }

    function test_a_spanish_morning_puts_the_day_before_the_month() {
        const date = new Date(2026, 10, 4, 8, 1)
        compare(Clock.dateText(date, spanish), "mié 4 nov")
        compare(Clock.timeText(date, spanish), "8:01 a. m.")
    }

    function test_noon_and_midnight_are_twelve() {
        compare(Clock.timeText(new Date(2026, 10, 4, 12, 0), english), "12:00 PM")
        compare(Clock.timeText(new Date(2026, 10, 4, 0, 5), english), "12:05 AM")
        compare(Clock.timeText(new Date(2026, 10, 4, 23, 59), spanish), "11:59 p. m.")
    }

    function test_a_24_hour_clock_has_no_am_or_pm() {
        const names = Object.assign({}, spanish, {uses24h: true})
        compare(Clock.timeText(new Date(2026, 10, 4, 20, 5), names), "20:05")
        compare(Clock.timeText(new Date(2026, 10, 4, 7, 5), names), "7:05")
        compare(Clock.timeText(new Date(2026, 10, 4, 7, 5), Object.assign({}, names, {padHours: true})), "07:05")
    }

    function test_abbreviations_lose_a_trailing_period() {
        compare(Clock.shortName("mié."), "mié")
        compare(Clock.shortName("nov."), "nov")
        compare(Clock.shortName("Wed"), "Wed")
    }

    function test_the_hour_format_follows_the_setting_over_the_region() {
        // Plasma's digital clock setting: 0 forces 12 h, 1 follows the region, 2 forces 24 h.
        verify(!Clock.uses24h(0, "HH:mm"))
        verify(Clock.uses24h(1, "HH:mm"))
        verify(!Clock.uses24h(1, "h:mm Ap"))
        verify(Clock.uses24h(2, "h:mm Ap"))
    }

    function test_the_text_changes_only_when_the_minute_does() {
        const a = new Date(2026, 10, 4, 8, 1, 0)
        const b = new Date(2026, 10, 4, 8, 1, 59)
        const c = new Date(2026, 10, 4, 8, 2, 0)
        compare(Clock.timeText(a, english), Clock.timeText(b, english))
        verify(Clock.timeText(b, english) !== Clock.timeText(c, english))
    }
}
