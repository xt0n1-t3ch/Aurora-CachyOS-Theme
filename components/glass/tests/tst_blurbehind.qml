// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Tests the blur region BlurBehind hands to KWin: each module's own rounded shape, in window coordinates.
// Run: QT_QPA_PLATFORM=offscreen qmltestrunner -import build/qml -input tests/tst_blurbehind.qml
import QtQuick
import QtTest
import org.aurora.glass

TestCase {
    id: test
    name: "BlurBehind"
    width: 420
    height: 320
    visible: true
    when: windowShown

    Item {
        id: stage
        x: 20
        y: 10
        width: 320
        height: 240
        // A 2x1 pill and a 2x2 card of the Control Center grid (62 pt cells, 12 pt gaps).
        Rectangle { id: pill; x: 10; y: 10; width: 136; height: 62; radius: 31 }
        Rectangle { id: card; x: 158; y: 10; width: 136; height: 136; radius: 24 }
        Item {
            id: zoomed
            x: 10; y: 160; width: 100; height: 40
            scale: 0.5
            transformOrigin: Item.TopLeft
            Rectangle { id: inner; anchors.fill: parent; radius: 20 }
        }
        BlurBehind { id: blur; sources: [pill, card, inner] }
    }

    // Without a source list, any item that says glassBlur: true is a source, wherever it sits in the window.
    Item {
        id: found
        x: 20; y: 260; width: 300; height: 50
        Item {
            x: 10; y: 5; width: 200; height: 40
            Rectangle { id: marked; property bool glassBlur: true; width: 100; height: 40; radius: 20 }
            Rectangle { id: unmarked; x: 120; width: 60; height: 40; radius: 20 }
        }
        BlurBehind { id: discover }
    }

    function test_items_marked_glassBlur_are_found_without_a_list() {
        tryVerify(() => discover.covers(30 + 50, 265 + 20), 2000, "the marked item's centre")
        verify(!discover.covers(150 + 30, 265 + 20), "an unmarked item stays clear")
        marked.visible = false
        tryVerify(() => !discover.covers(30 + 50, 265 + 20), 2000, "hiding it drops it")
        marked.visible = true
    }

    function init() {
        pill.visible = true
        card.visible = true
        card.opacity = 1
        // The region follows on the next frame; wait until every module is back in it.
        tryVerify(() => blur.covers(30 + 68, 20 + 31) && blur.covers(178 + 68, 20 + 68))
    }

    function test_a_pill_is_blurred_up_to_its_round_ends_and_not_past_them() {
        // The pill sits at window (30, 20), 136 x 62.
        verify(blur.covers(30 + 68, 20 + 31), "centre")
        verify(blur.covers(30 + 1, 20 + 31), "middle of the round left end")
        verify(!blur.covers(30 + 2, 20 + 2), "corner outside the round left end")
        verify(!blur.covers(30 + 134, 20 + 60), "corner outside the round right end")
    }

    function test_the_gap_between_two_modules_stays_clear() {
        verify(!blur.covers(30 + 136 + 6, 20 + 31), "the 12 pt gap")
        verify(blur.covers(178 + 68, 20 + 68), "centre of the card")
    }

    function test_a_hidden_module_drops_out_of_the_region() {
        card.visible = false
        tryVerify(() => !blur.covers(178 + 68, 20 + 68))
    }

    function test_a_transparent_module_drops_out_of_the_region() {
        card.opacity = 0
        tryVerify(() => !blur.covers(178 + 68, 20 + 68))
    }

    function test_a_scaled_module_keeps_its_shape_at_its_scale() {
        // 100 x 40 at scale 0.5 from (30, 170): 50 x 20 on screen, radius 10.
        verify(blur.covers(30 + 25, 170 + 10), "centre")
        verify(!blur.covers(30 + 60, 170 + 10), "past the scaled width")
        verify(!blur.covers(30 + 1, 170 + 1), "outside the scaled round corner")
    }

    function test_the_bounds_hold_every_visible_module() {
        compare(blur.bounds.x, 30)
        compare(blur.bounds.y, 20)
        compare(blur.bounds.x + blur.bounds.width, 178 + 136)
        compare(blur.bounds.y + blur.bounds.height, 170 + 20)
    }
}
