/*
 *  SPDX-FileCopyrightText: 2020 Kpple <info.kpple@gmail.com>
 *  SPDX-FileCopyrightText: 2024 Christian Tallner <chrtall@gmx.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2

import org.kde.plasma.core as PlasmaCore
import org.kde.ksvg as KSvg
import org.kde.iconthemes as KIconThemes
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.kde.plasma.plasmoid

import "../code/tools.js" as Tools


KCM.SimpleKCM {
    id: root

    property string cfg_icon: Plasmoid.configuration.icon
    // Aurora: whether a logo that has colors shows them; black and white (the default) when it is off.
    property bool logoColors: {
        const logo = Tools.logoOf(Plasmoid.configuration.icon);
        return logo !== null && logo.colored;
    }
    // The logo the chosen icon belongs to, or null when the user picked another icon.
    readonly property var chosenLogo: Tools.logoOf(cfg_icon)
    
    property alias cfg_shortcutLabelStyle: shortcutLabelStyle.currentIndex
    property alias cfg_automaticWidth: automaticWidth.checked
    property alias cfg_automaticHeight: automaticHeight.checked
    property alias cfg_menuWidth: menuWidth.value
    property alias cfg_buttonHeight: buttonHeight.value


    Kirigami.FormLayout {
        // Aurora: the menu bar logo, picked from Aurora's logos. Each tile shows the logo on a slice of the dark
        // menu bar, as it will look there; "Other icon" below takes any icon instead.
        Flow {
            id: logoTiles
            Kirigami.FormData.label: i18n("Logo:")
            Layout.preferredWidth: (Kirigami.Units.gridUnit * 5 + spacing) * Tools.logos.length
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: Tools.logos

                delegate: QQC2.AbstractButton {
                    id: tile
                    required property var modelData
                    readonly property bool chosen: root.chosenLogo !== null && root.chosenLogo.id === modelData.id

                    width: Kirigami.Units.gridUnit * 5
                    height: Kirigami.Units.gridUnit * 5
                    hoverEnabled: true
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.name
                    Accessible.checked: chosen
                    onClicked: root.cfg_icon = Tools.logoIcon(modelData.id, root.logoColors)

                    background: Rectangle {
                        radius: Kirigami.Units.cornerRadius * 2
                        color: tile.chosen ? Qt.alpha(Kirigami.Theme.highlightColor, 0.18)
                             : tile.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.07) : "transparent"
                        border.width: tile.chosen || tile.visualFocus ? 2 : 0
                        border.color: Kirigami.Theme.highlightColor
                    }

                    contentItem: Column {
                        spacing: Kirigami.Units.smallSpacing
                        topPadding: Kirigami.Units.smallSpacing

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: Kirigami.Units.gridUnit * 3.4
                            height: Kirigami.Units.gridUnit * 2.4
                            radius: Kirigami.Units.cornerRadius * 1.5
                            color: "#29233f"
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.12)
                            // The symbolic logos take the text color of a dark surface here, as on the menu bar.
                            Kirigami.Theme.colorSet: Kirigami.Theme.Complementary
                            Kirigami.Theme.inherit: false

                            Kirigami.Icon {
                                anchors.centerIn: parent
                                width: Kirigami.Units.iconSizes.smallMedium
                                height: width
                                source: Tools.logoIcon(tile.modelData.id, root.logoColors)
                            }
                        }
                        QQC2.Label {
                            width: tile.width - Kirigami.Units.smallSpacing * 2
                            anchors.horizontalCenter: parent.horizontalCenter
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            text: tile.modelData.name
                            font.weight: tile.chosen ? Font.DemiBold : Font.Normal
                        }
                    }
                }
            }
        }

        // Aurora: switches the chosen logo to its own colors; it is enabled only for logos that have them.
        QQC2.CheckBox {
            text: i18n("Use the logo's colors")
            checked: root.logoColors
            enabled: root.chosenLogo !== null && Tools.hasColors(root.chosenLogo.id)
            onToggled: {
                root.logoColors = checked;
                if (root.chosenLogo !== null) {
                    root.cfg_icon = Tools.logoIcon(root.chosenLogo.id, checked);
                }
            }
        }

        /*
            *  Begin: code from plasma-desktop/applets/kickoff/contents/ui/ConfigGeneral.qml
            *  SPDX-FileCopyrightText: 2013 David Edmundson <davidedmundson@kde.org>
            *  SPDX-FileCopyrightText: 2021 Mikel Johnson <mikel5764@gmail.com>
            *  SPDX-FileCopyrightText: 2022 Nate Graham <nate@kde.org>
            *  SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>
            *
            *  SPDX-License-Identifier: GPL-2.0-or-later
         */
        QQC2.Button {
            id: iconButton

            Kirigami.FormData.label: i18n("Other icon:")

            implicitWidth: previewFrame.width + Kirigami.Units.smallSpacing * 2
            implicitHeight: previewFrame.height + Kirigami.Units.smallSpacing * 2
            hoverEnabled: true

            Accessible.name: i18nc("@action:button", "Change Kpple Menu's icon")
            Accessible.description: i18nc("@info:whatsthis", "Current icon is %1. Click to open menu to change the current icon or reset to the default icon.", root.cfg_icon)
            Accessible.role: Accessible.ButtonMenu

            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            QQC2.ToolTip.text: i18nc("@info:tooltip", "Icon name is \"%1\"", root.cfg_icon)
            QQC2.ToolTip.visible: iconButton.hovered && root.cfg_icon.length > 0

            KIconThemes.IconDialog {
                id: iconDialog
                onIconNameChanged: (iconName) => {
                    root.cfg_icon = iconName || Tools.defaultIconName;
                }
            }

            onPressed: iconMenu.opened ? iconMenu.close() : iconMenu.open()

            KSvg.FrameSvgItem {
                id: previewFrame
                anchors.centerIn: parent
                imagePath: Plasmoid.formFactor === PlasmaCore.Types.Vertical || Plasmoid.formFactor === PlasmaCore.Types.Horizontal
                    ? "widgets/panel-background" : "widgets/background"
                width: Kirigami.Units.iconSizes.large + fixedMargins.left + fixedMargins.right
                height: Kirigami.Units.iconSizes.large + fixedMargins.top + fixedMargins.bottom

                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: Kirigami.Units.iconSizes.large
                    height: Kirigami.Units.iconSizes.large
                    source: Tools.iconOrDefault(Plasmoid.formFactor, root.cfg_icon)
                }
            }

            QQC2.Menu {
                id: iconMenu

                // Appear below the button
                y: parent.height

                QQC2.MenuItem {
                    text: i18nc("@item:inmenu Open icon chooser dialog", "Choose…")
                    icon.name: "document-open-folder"
                    Accessible.description: i18nc("@info:whatsthis", "Choose an icon for Application Launcher")
                    onClicked: iconDialog.open()
                }
                QQC2.MenuItem {
                    text: i18nc("@item:inmenu Reset icon to default", "Reset to default icon")
                    icon.name: "edit-clear"
                    enabled: root.cfg_icon !== Tools.defaultIconName
                    onClicked: root.cfg_icon = Tools.defaultIconName
                }
                QQC2.MenuItem {
                    text: i18nc("@action:inmenu", "Remove icon")
                    icon.name: "delete"
                    enabled: root.cfg_icon !== "" && Plasmoid.formFactor !== PlasmaCore.Types.Vertical
                    onClicked: root.cfg_icon = ""
                }
            }
        }
        /*
            End: code from plasma-desktop/applets/kickoff/contents/ui/ConfigGeneral.qml
        */
        
        QQC2.ComboBox {
            id: shortcutLabelStyle
            Kirigami.FormData.label: i18n("Shortcut labels:")
            model: [i18n("Apple symbols"), i18n("PC labels")]
        }

        // Width and item-height settings, including the 240/28 px manual defaults,
        // are inspired by 51n7/kMenu's ConfigGeneral.qml and config/main.xml
        // (GPL-2.0-or-later):
        // https://github.com/51n7/kMenu/blob/915bfc9d809fe30a6502dc9fd7f4f4d7f140d8d4/package/contents/ui/config/ConfigGeneral.qml
        // https://github.com/51n7/kMenu/blob/915bfc9d809fe30a6502dc9fd7f4f4d7f140d8d4/package/contents/config/main.xml
        // The menu adds separate automatic-sizing checkboxes for each dimension.
        QQC2.CheckBox {
            id: automaticWidth
            objectName: "automaticWidthSetting"
            text: i18n("Use automatic width")
            checked: true
        }

        QQC2.SpinBox {
            id: menuWidth
            objectName: "menuWidthSetting"
            Kirigami.FormData.label: i18n("Menu width:")
            enabled: !automaticWidth.checked
            from: 1
            to: 10000
            value: 240
            editable: true
        }

        QQC2.CheckBox {
            id: automaticHeight
            objectName: "automaticHeightSetting"
            text: i18n("Use automatic item height")
            checked: true
        }

        QQC2.SpinBox {
            id: buttonHeight
            objectName: "menuItemHeightSetting"
            Kirigami.FormData.label: i18n("Menu item height:")
            enabled: !automaticHeight.checked
            from: 1
            to: 10000
            value: 28
            editable: true
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Sizes are in pixels.")
            wrapMode: Text.WordWrap
        }

    }
}
