// SPDX-License-Identifier: GPL-3.0-only
// The Control Center's own rules: the Tahoe grid, what each module says, when a click opens the window.
// Run: QT_QPA_PLATFORM=offscreen qmltestrunner -input tests/tst_cc.qml
// The grid metrics come from macOS Tahoe and macOS 27 screenshots at 2x (Ars Technica, 2026-06-11; AppleInsider,
// 2026-03-06): 62 pt cells, 12 pt gaps, four columns.
import QtQuick
import QtTest
import "../package/contents/ui/js/cc.js" as CC

TestCase {
    name: "ControlCenter"

    function test_the_grid_is_four_62pt_cells_with_12pt_gaps() {
        compare(CC.gridWidth(), 284)
        compare(CC.rect(0, 0, 2, 1), {x: 0, y: 0, width: 136, height: 62})
        compare(CC.rect(2, 0, 2, 2), {x: 148, y: 0, width: 136, height: 136})
        compare(CC.rect(3, 2, 1, 1), {x: 222, y: 148, width: 62, height: 62})
    }

    // The rectangle of one module in a layout, or null when it is not there.
    function place(placements, id) {
        const found = placements.filter(p => p.id === id)
        return found.length ? {x: found[0].x, y: found[0].y, width: found[0].width, height: found[0].height} : null
    }

    function test_the_default_layout_follows_tahoe() {
        const all = CC.layout([])
        compare(place(all, "network"), {x: 0, y: 0, width: 136, height: 62})
        compare(place(all, "bluetooth"), {x: 0, y: 74, width: 136, height: 62})
        compare(place(all, "nightlight"), {x: 0, y: 148, width: 136, height: 62})
        compare(place(all, "media"), {x: 148, y: 0, width: 136, height: 136})
        compare(place(all, "overview"), {x: 148, y: 148, width: 62, height: 62})
        compare(place(all, "mirroring"), {x: 222, y: 148, width: 62, height: 62})
        compare(place(all, "settings"), {x: 0, y: 222, width: 62, height: 62})
        compare(place(all, "screenshot"), {x: 74, y: 222, width: 62, height: 62})
        compare(place(all, "focus"), {x: 148, y: 222, width: 136, height: 62})
        compare(place(all, "display"), {x: 0, y: 296, width: 284, height: 62})
        compare(place(all, "sound"), {x: 0, y: 370, width: 284, height: 62})
        compare(CC.heightOf(all), 432)
    }

    function test_a_hidden_display_lets_sound_move_up() {
        const placements = CC.layout(["display"])
        compare(place(placements, "display"), null)
        compare(place(placements, "sound"), {x: 0, y: 296, width: 284, height: 62})
        compare(CC.heightOf(placements), 358)
    }

    // Whether two placements share any area.
    function overlaps(a, b) {
        return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height
    }

    function test_no_two_modules_overlap_whatever_is_hidden() {
        const optional = ["media", "nightlight", "display", "sound", "focus"]
        for (let mask = 0; mask < (1 << optional.length); ++mask) {
            const hidden = optional.filter((id, i) => mask & (1 << i))
            const placements = CC.layout(hidden)
            for (let i = 0; i < placements.length; ++i) {
                verify(placements[i].x + placements[i].width <= CC.gridWidth(), "inside the grid")
                for (let j = i + 1; j < placements.length; ++j)
                    verify(!overlaps(placements[i], placements[j]), placements[i].id + " / " + placements[j].id + " with " + hidden)
            }
        }
    }

    // Icon names are plasma-nm 6.8.2's (libs/connectionicon.cpp): network-wired-activated, network-wired-disconnected,
    // network-wireless-connected-NN, network-wireless-disconnected, network-unavailable.
    function test_the_network_pill_names_ethernet_when_wired() {
        compare(CC.networkSummary({wifi: false, wifiOn: false, icon: "network-wired-activated", status: "Wired connection 2: Connected"}),
                {title: "Ethernet", subtitle: "Connected", glyph: "ethernet", active: true})
    }

    function test_the_network_pill_names_the_wifi_network_it_is_on() {
        compare(CC.networkSummary({wifi: true, wifiOn: true, icon: "network-wireless-connected-80", status: "HomeNet: Connected"}),
                {title: "Wi-Fi", subtitle: "HomeNet", glyph: "wifi", active: true})
    }

    function test_the_network_pill_says_off_when_wifi_is_off() {
        compare(CC.networkSummary({wifi: true, wifiOn: false, icon: "network-wireless-disconnected", status: ""}),
                {title: "Wi-Fi", subtitle: "Off", glyph: "wifi", active: false})
    }

    function test_the_network_pill_says_not_connected_without_a_link() {
        compare(CC.networkSummary({wifi: false, wifiOn: false, icon: "network-wired-disconnected", status: ""}),
                {title: "Ethernet", subtitle: "Not Connected", glyph: "ethernet", active: false})
        compare(CC.networkSummary({wifi: false, wifiOn: false, icon: "network-unavailable", status: ""}),
                {title: "Ethernet", subtitle: "Not Connected", glyph: "ethernet", active: false})
        compare(CC.networkSummary({wifi: true, wifiOn: true, icon: "network-wireless-disconnected", status: ""}),
                {title: "Wi-Fi", subtitle: "Not Connected", glyph: "wifi", active: true})
    }

    function test_the_connection_name_is_read_from_the_status_text() {
        compare(CC.connectionName("HomeNet: Connected"), "HomeNet")
        compare(CC.connectionName("Work: Connected\nVPN: Connected"), "Work")
        compare(CC.connectionName(""), "")
    }

    function test_bluetooth_says_what_it_is_doing() {
        compare(CC.bluetoothSummary({adapters: 0, operational: false, connected: []}), {subtitle: "Unavailable", active: false, available: false})
        compare(CC.bluetoothSummary({adapters: 1, operational: false, connected: []}), {subtitle: "Off", active: false, available: true})
        compare(CC.bluetoothSummary({adapters: 1, operational: true, connected: []}), {subtitle: "On", active: true, available: true})
        compare(CC.bluetoothSummary({adapters: 1, operational: true, connected: ["GH50", "Mouse"]}), {subtitle: "GH50", active: true, available: true})
    }

    function test_volume_percent_and_volume_round_trip() {
        const normal = 65536
        for (let p = 0; p <= 100; ++p)
            compare(CC.percentFromVolume(CC.volumeFromPercent(p, normal), normal), p)
        compare(CC.percentFromVolume(normal * 1.5, normal), 100, "boosted volume shows as full")
        compare(CC.volumeFromPercent(-5, normal), 0)
    }

    function test_the_speaker_glyph_follows_mute_and_level() {
        compare(CC.speakerGlyph(40, false), "speaker")
        compare(CC.speakerGlyph(40, true), "speaker.slash")
        compare(CC.speakerGlyph(0, false), "speaker.slash")
    }

    function test_one_display_slider_drives_every_monitor() {
        const displays = [{name: "a", current: 50, maximum: 100}, {name: "b", current: 20, maximum: 40}, {name: "c", current: 0, maximum: 0}]
        compare(CC.brightnessPercent(displays), 50, "the average of the monitors that report a range")
        compare(CC.brightnessTargets(80, displays), [{name: "a", value: 80}, {name: "b", value: 32}])
        compare(CC.brightnessPercent([]), -1, "no monitor: no slider")
    }

    function test_a_click_on_the_icon_toggles_the_window() {
        verify(CC.opensOnClick(false, 10000), "closed: a click opens it")
        verify(!CC.opensOnClick(true, 10000), "open: a click closes it")
        verify(!CC.opensOnClick(false, 80), "the click that just closed it by focus loss does not reopen it")
    }

    function test_the_window_hangs_below_the_bar_at_the_right_edge() {
        // A 2560 x 1440 screen at x 1920 with a 32 px menu bar. The window starts right under the bar and ends at the
        // screen's edge: KWin keeps popups out of the bar, and past the edge is the next screen.
        compare(CC.windowPosition({x: 1920, y: 0, width: 2560, height: 1440}, 32, 284 + CC.MARGINS.left + CC.MARGINS.right),
                {x: 1920 + 2560 - 284 - 28 - 10, y: 32})
        compare(CC.MARGINS, {top: 6, right: 10, bottom: 28, left: 28})
    }

    function test_now_playing_falls_back_to_not_playing() {
        compare(CC.mediaSummary(null), {title: "Not Playing", artist: "", hasPlayer: false})
        compare(CC.mediaSummary({track: "", artist: "", identity: "Limusic"}), {title: "Limusic", artist: "", hasPlayer: true})
        compare(CC.mediaSummary({track: "Song", artist: "Band", identity: "Edge"}), {title: "Song", artist: "Band", hasPlayer: true})
    }

    // A pill's title fits between the round switch and the pill's rounded end: "Night Light", the widest default title
    // (70.7 px in SF Pro Text Semibold at 13 px, 68.7 px in Inter), keeps its size, and the text stays 10 px clear of
    // the end, where the curve starts.
    function test_a_pill_title_fits_clear_of_the_rounded_end() {
        const text = CC.pillText(136, 62, 36)
        verify(text.width >= 71, "title width " + text.width)
        verify(136 - (text.x + text.width) >= 10, "right inset " + (136 - (text.x + text.width)))
        compare(text.x, 13 + 36 + 6)
    }
}
