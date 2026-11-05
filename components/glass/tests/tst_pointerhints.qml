// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Every clickable thing in plasmashell shows the hand: while a PointerHints lives, the item a resting pointer is
// over gets the pointing hand when it takes left clicks (a Qt Quick Controls button, an item with an enabled
// left-button TapHandler, a hover-enabled MouseArea). Items with their own cursor keep it; areas that take no
// left click keep the arrow.
// Run: QT_QPA_PLATFORM=offscreen qmltestrunner -import build/qml -input tests/tst_pointerhints.qml
import QtQuick
import QtQuick.Controls
import QtTest
import org.aurora.glass

TestCase {
    id: test
    name: "PointerHints"
    width: 400
    height: 300
    visible: true
    when: windowShown

    PointerHints { id: hints }

    Rectangle { id: tapTarget; x: 10; y: 10; width: 60; height: 40; TapHandler { acceptedButtons: Qt.LeftButton } }
    MouseArea { id: hoverArea; x: 80; y: 10; width: 60; height: 40; hoverEnabled: true; onClicked: {} }
    // A MouseArea without hover is how Plasma catches the wheel or keeps presses from leaking: no hand.
    MouseArea { id: plainArea; x: 150; y: 10; width: 60; height: 40 }
    Button { id: button; x: 220; y: 10; width: 80; height: 40; text: "OK" }
    TextInput { id: field; x: 10; y: 70; width: 120; height: 30; text: "type here" }
    Rectangle { id: rightOnly; x: 150; y: 70; width: 60; height: 40; TapHandler { acceptedButtons: Qt.RightButton } }
    Rectangle { id: disabledTap; x: 220; y: 70; width: 60; height: 40; TapHandler { enabled: false } }
    Button { id: disabledButton; x: 300; y: 70; width: 80; height: 40; text: "No"; enabled: false }
    Item {
        id: outer
        x: 10; y: 130; width: 200; height: 100
        TapHandler {}
        Rectangle { id: innerChild; x: 20; y: 20; width: 50; height: 50 }
    }
    // The tray's pattern (AbstractItem.qml): the icon and a MouseArea filling the item are siblings, the icon on top.
    Item {
        id: trayItem
        x: 10; y: 240; width: 40; height: 40
        MouseArea { id: trayArea; anchors.fill: parent; hoverEnabled: true }
        Rectangle { id: trayIcon; anchors.centerIn: parent; width: 22; height: 22 }
    }
    MenuItem { id: menuItem; x: 250; y: 220; width: 120; height: 30; text: "Open" }
    MouseArea {
        id: ownCursor
        x: 250; y: 130; width: 100; height: 60
        hoverEnabled: true
        cursorShape: Qt.SizeAllCursor
    }

    // Moves the pointer to the centre of `item` in two steps with a pause after each, as the hand filter throttles
    // moves that come too close together.
    function hover(item) {
        const p = item.mapToItem(null, item.width / 2, item.height / 2)
        mouseMove(test, p.x - 3, p.y - 3)
        wait(20)
        mouseMove(test, p.x, p.y)
        wait(20)
    }

    function test_an_item_with_a_tap_handler_shows_the_hand() {
        hover(tapTarget)
        compare(hints.cursorShapeOf(tapTarget), Qt.PointingHandCursor)
    }

    function test_a_hover_mouse_area_shows_the_hand() {
        hover(hoverArea)
        compare(hoverArea.cursorShape, Qt.PointingHandCursor)
    }

    function test_a_button_shows_the_hand() {
        hover(button)
        compare(hints.cursorShapeOf(button), Qt.PointingHandCursor)
    }

    function test_a_mouse_area_without_hover_keeps_the_arrow() {
        hover(plainArea)
        compare(plainArea.cursorShape, Qt.ArrowCursor)
    }

    function test_a_text_field_never_gets_the_hand() {
        const before = hints.cursorShapeOf(field)
        hover(field)
        compare(hints.cursorShapeOf(field), before)
        verify(hints.cursorShapeOf(field) !== Qt.PointingHandCursor)
    }

    function test_a_right_click_only_handler_keeps_the_arrow() {
        hover(rightOnly)
        compare(hints.cursorShapeOf(rightOnly), Qt.ArrowCursor)
    }

    function test_disabled_controls_keep_the_arrow() {
        hover(disabledTap)
        compare(hints.cursorShapeOf(disabledTap), Qt.ArrowCursor)
        hover(disabledButton)
        compare(hints.cursorShapeOf(disabledButton), Qt.ArrowCursor)
    }

    function test_the_hand_goes_on_the_ancestor_that_takes_the_click() {
        hover(innerChild)
        compare(hints.cursorShapeOf(outer), Qt.PointingHandCursor)
        compare(hints.cursorShapeOf(innerChild), Qt.ArrowCursor)
    }

    function test_a_click_area_filling_the_parent_gets_the_hand_under_its_icon() {
        hover(trayIcon)
        compare(trayArea.cursorShape, Qt.PointingHandCursor)
    }

    function test_menu_items_keep_the_arrow_as_on_macos() {
        hover(menuItem)
        compare(hints.cursorShapeOf(menuItem), Qt.ArrowCursor)
    }

    function test_an_item_with_its_own_cursor_keeps_it() {
        hover(ownCursor)
        compare(ownCursor.cursorShape, Qt.SizeAllCursor)
    }
}
