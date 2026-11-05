// SPDX-License-Identifier: GPL-3.0-only
// Edit Controls: which optional modules the Control Center shows.
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Kirigami.FormLayout {
    property alias cfg_showMediaPlayer: media.checked
    property alias cfg_showNightColor: nightLight.checked
    property alias cfg_showDnd: focusModule.checked
    property alias cfg_showBrightness: display.checked
    property alias cfg_showVolume: sound.checked

    QQC2.CheckBox { id: media; Kirigami.FormData.label: "Show:"; text: "Now Playing" }
    QQC2.CheckBox { id: nightLight; text: "Night Light" }
    QQC2.CheckBox { id: focusModule; text: "Focus" }
    QQC2.CheckBox { id: display; text: "Display brightness" }
    QQC2.CheckBox { id: sound; text: "Sound" }
}
