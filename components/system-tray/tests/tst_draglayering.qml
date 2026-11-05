// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The layering AbstractItem.qml relies on for dragging tray icons, checked against Qt itself: a click still reaches
// the icon, and a drag past the threshold goes to the handler. Run: qmltestrunner -input tst_draglayering.qml
import QtQuick
import QtTest

Item {
    id: scene
    width: 200
    height: 100

    // Stands in for AbstractItem: an icon that handles its own clicks, as a plasmoid's compact
    // representation or the item's MouseArea does.
    Item {
        id: abstractItem
        anchors.fill: parent

        MouseArea {
            id: icon
            anchors.fill: parent
            property int clicks: 0
            onClicked: clicks += 1
        }

        // A DragHandler accepts a mouse press (qquickdraghandler.cpp:264, Qt 6.12.0), and every item under one that
        // accepted gets only handler delivery (qquickdeliveryagent.cpp:2548). So the handler belongs to the icon's
        // parent: the icon's own MouseArea takes the click first, and the handler still sees the press and takes the
        // pointer over once a drag passes the threshold.
        DragHandler {
            id: dragHandler
            target: null
            acceptedButtons: Qt.LeftButton
        }
    }

    SignalSpy {
        id: dragStarts
        target: dragHandler
        signalName: "activeChanged"
    }

    TestCase {
        name: "DragLayering"
        when: windowShown

        function init() {
            icon.clicks = 0;
            dragStarts.clear();
        }

        function test_a_click_reaches_the_icon() {
            mouseClick(icon, 100, 50);
            compare(icon.clicks, 1);
            compare(dragStarts.count, 0);
        }

        function test_a_drag_past_the_threshold_starts_the_handler_and_is_no_click() {
            const step = Application.styleHints.startDragDistance + 4;
            mousePress(icon, 100, 50);
            for (let i = 1; i <= 4; ++i) {
                mouseMove(icon, 100 + i * step, 50);
            }
            verify(dragHandler.active);
            mouseRelease(icon, 100 + 4 * step, 50);
            compare(icon.clicks, 0);
        }

        function test_a_drag_under_the_threshold_is_still_a_click() {
            mousePress(icon, 100, 50);
            mouseMove(icon, 101, 50);
            mouseRelease(icon, 101, 50);
            compare(icon.clicks, 1);
            compare(dragStarts.count, 0);
        }
    }
}
