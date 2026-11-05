// SPDX-License-Identifier: GPL-3.0-only
// The window the menu bar icon opens: transparent, so the modules float on their own glass like macOS Tahoe's
// Control Center, hung below the bar at the right edge of the screen.
import QtQuick
import QtQuick.Window
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import "lib"
import "js/cc.js" as CC

PlasmaCore.Dialog {
    id: window

    // The menu bar icon the window hangs from. Its window gives the bar height and its screen the position.
    property Item anchorItem
    // When the window last hid, in ms since the epoch. CC.opensOnClick uses it to ignore the click that follows a
    // focus loss.
    property double hiddenAt: 0

    // Opens the window, or closes it when it is open.
    function toggle() {
        if (!CC.opensOnClick(visible, Date.now() - hiddenAt)) {
            visible = false
            return
        }
        place()
        content.refresh()
        visible = true
        requestActivate()
        frame.forceActiveFocus()
        intro.restart()
    }

    // Puts the window right under the menu bar, at the right edge of the bar's screen.
    function place() {
        const bar = anchorItem ? anchorItem.Window.window : null
        if (!bar) {
            return
        }
        // QWindow.screen is not readable from QML; the attached Screen of the icon is the bar's screen.
        const screen = anchorItem.Screen
        const at = CC.windowPosition({x: screen.virtualX, y: screen.virtualY, width: screen.width, height: screen.height},
                                     bar.height, frame.width)
        x = at.x
        y = at.y
    }

    type: PlasmaCore.Dialog.AppletPopup
    location: PlasmaCore.Types.Floating
    // Every module is its own Glass, so the window itself draws nothing.
    backgroundHints: PlasmaCore.Dialog.NoBackground
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    hideOnWindowDeactivate: true
    visible: false
    onVisibleChanged: {
        if (!visible) {
            hiddenAt = Date.now()
            content.panel = ""
        }
    }

    mainItem: Item {
        id: frame
        width: content.implicitWidth + CC.MARGINS.left + CC.MARGINS.right
        height: content.implicitHeight + CC.MARGINS.top + CC.MARGINS.bottom
        focus: true
        Keys.onEscapePressed: {
            if (content.panel.length) {
                content.panel = ""
            } else {
                window.visible = false
            }
        }

        // A click beside the modules goes back from a panel, or closes the Control Center.
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (content.panel.length) {
                    content.panel = ""
                } else {
                    window.visible = false
                }
            }
        }

        ControlCenter {
            id: content
            x: CC.MARGINS.left
            y: CC.MARGINS.top
            width: implicitWidth
            height: implicitHeight
            transformOrigin: Item.TopRight
            onCloseRequested: window.visible = false
        }

        GlassBlur {}

        // The macOS drop from the menu bar, at the desktop's animation speed (cc.js entrance).
        readonly property var motion: CC.entrance(Kirigami.Units.longDuration)
        ParallelAnimation {
            id: intro
            NumberAnimation { target: content; property: "opacity"; from: 0; to: 1; duration: frame.motion.fadeDuration; easing.type: Easing.OutCubic }
            NumberAnimation { target: content; property: "scale"; from: frame.motion.scaleFrom; to: 1; duration: frame.motion.scaleDuration; easing.type: Easing.OutQuint }
        }
    }
}
