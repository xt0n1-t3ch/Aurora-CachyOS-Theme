// SPDX-License-Identifier: GPL-2.0-or-later
// Info Center > Energy on a desktop without a battery: what the processor and the graphics card draw now, the last
// two minutes, the power profile, and the energy used while the page is open.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import org.aurora.energy

KCM.SimpleKCM {
    id: root

    // The colors that tie each sensor's dot and chart line together.
    readonly property color cpuColor: Kirigami.Theme.highlightColor
    readonly property color gpuColor: "#30d158"
    readonly property bool anySensor: power.cpuAvailable || power.gpuAvailable

    // Formats a power value: whole watts from 100 W up, one decimal below.
    function watts(value) {
        return value >= 100 ? Math.round(value) + " W" : value.toFixed(1) + " W"
    }

    PowerMonitor {
        id: power
        // Sampling runs only while the page is on screen.
        active: root.visible
    }

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        visible: !root.anySensor
        icon.name: "battery"
        text: i18nc("@info", "No power sensors can be read on this system")
        explanation: i18nc("@info", "The processor's RAPL counters or the NVIDIA driver are needed to measure power.")
    }

    ColumnLayout {
        visible: root.anySensor
        width: parent.width
        spacing: Kirigami.Units.largeSpacing * 2

        // The headline: everything measured, right now.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            spacing: 0
            QQC2.Label {
                text: i18nc("@label", "Drawing now")
                opacity: 0.7
            }
            Kirigami.Heading {
                level: 1
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 3
                font.weight: Font.DemiBold
                text: root.watts(power.cpuWatts + power.gpuWatts)
            }
            QQC2.Label {
                text: i18nc("@info", "Processor and graphics card, measured every second")
                opacity: 0.7
            }
        }

        GridLayout {
            Layout.fillWidth: true
            // Two cards side by side when the page is wide enough.
            columns: root.width > Kirigami.Units.gridUnit * 30 ? 2 : 1
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.largeSpacing

            // One card per sensor; a sensor that cannot be read hides its card.
            Repeater {
                model: [
                    { title: i18nc("@title", "Processor"), detail: i18nc("@info", "CPU package"), color: root.cpuColor,
                      available: power.cpuAvailable, watts: power.cpuWatts, temperature: power.cpuTemperature },
                    { title: i18nc("@title", "Graphics card"), detail: power.gpuName, color: root.gpuColor,
                      available: power.gpuAvailable, watts: power.gpuWatts, temperature: power.gpuTemperature }
                ]
                delegate: Kirigami.AbstractCard {
                    required property var modelData
                    Layout.fillWidth: true
                    visible: modelData.available
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: Kirigami.Units.smallSpacing
                            width: Kirigami.Units.smallSpacing * 2
                            height: width
                            radius: width / 2
                            color: modelData.color
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            QQC2.Label {
                                text: modelData.title
                                font.weight: Font.DemiBold
                            }
                            QQC2.Label {
                                Layout.fillWidth: true
                                text: modelData.detail
                                elide: Text.ElideRight
                                opacity: 0.7
                            }
                        }
                        ColumnLayout {
                            spacing: 0
                            Kirigami.Heading {
                                Layout.alignment: Qt.AlignRight
                                level: 2
                                text: root.watts(modelData.watts)
                            }
                            QQC2.Label {
                                Layout.alignment: Qt.AlignRight
                                visible: modelData.temperature > 0
                                text: i18nc("@info temperature", "%1 °C", Math.round(modelData.temperature))
                                opacity: 0.7
                            }
                        }
                    }
                }
            }
        }

        Kirigami.AbstractCard {
            Layout.fillWidth: true
            header: RowLayout {
                spacing: Kirigami.Units.largeSpacing
                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 4
                    text: i18nc("@title", "Last two minutes")
                }
                Repeater {
                    model: [
                        { name: i18nc("@label", "Processor"), color: root.cpuColor, shown: power.cpuAvailable },
                        { name: i18nc("@label", "Graphics card"), color: root.gpuColor, shown: power.gpuAvailable }
                    ]
                    delegate: RowLayout {
                        required property var modelData
                        visible: modelData.shown
                        spacing: Kirigami.Units.smallSpacing
                        Rectangle {
                            width: Kirigami.Units.smallSpacing * 2
                            height: width
                            radius: width / 2
                            color: modelData.color
                        }
                        QQC2.Label {
                            text: modelData.name
                            opacity: 0.7
                        }
                    }
                }
            }
            contentItem: Canvas {
                id: chart
                implicitHeight: Kirigami.Units.gridUnit * 9

                // Draws one history as a line. `top` is the watt value at the top edge; the newest sample sits at the
                // right edge, so a short history starts part way across.
                function line(ctx, history, top, color) {
                    if (history.length < 2) {
                        return
                    }
                    const step = width / (power.historyLength - 1)
                    const x0 = width - (history.length - 1) * step
                    ctx.beginPath()
                    for (let i = 0; i < history.length; ++i) {
                        const x = x0 + i * step
                        const y = height - history[i] / top * height
                        if (i === 0) {
                            ctx.moveTo(x, y)
                        } else {
                            ctx.lineTo(x, y)
                        }
                    }
                    ctx.strokeStyle = color
                    ctx.lineWidth = 2
                    ctx.lineJoin = "round"
                    ctx.stroke()
                }

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const cpu = power.cpuHistory
                    const gpu = power.gpuHistory
                    // The scale is the busiest second (10 W at least) plus 10%, rounded up to a multiple of 25 W, so
                    // lines do not touch the top.
                    const busiest = Math.max(10, ...cpu, ...gpu)
                    const top = Math.ceil(busiest * 1.1 / 25) * 25
                    ctx.strokeStyle = Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)
                    ctx.lineWidth = 1
                    // Guide lines at a quarter, a half and three quarters of the height.
                    for (let i = 1; i < 4; ++i) {
                        const y = Math.round(height * i / 4) + 0.5
                        ctx.beginPath()
                        ctx.moveTo(0, y)
                        ctx.lineTo(width, y)
                        ctx.stroke()
                    }
                    ctx.fillStyle = Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.55)
                    ctx.font = Math.round(Kirigami.Theme.smallFont.pixelSize > 0 ? Kirigami.Theme.smallFont.pixelSize : 11) + "px sans-serif"
                    ctx.fillText(top + " W", 2, 12)
                    if (power.cpuAvailable) {
                        line(ctx, cpu, top, root.cpuColor)
                    }
                    if (power.gpuAvailable) {
                        line(ctx, gpu, top, root.gpuColor)
                    }
                }

                Connections {
                    target: power
                    function onSampled() { chart.requestPaint() }
                }
            }
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true
            visible: power.profiles.length > 0

            RowLayout {
                Kirigami.FormData.label: i18nc("@label", "Power profile:")
                spacing: Kirigami.Units.smallSpacing
                QQC2.ButtonGroup { id: profileGroup }
                Repeater {
                    model: power.profiles
                    delegate: QQC2.Button {
                        required property string modelData
                        checkable: true
                        checked: power.profile === modelData
                        QQC2.ButtonGroup.group: profileGroup
                        // A profile the page has no name for shows the daemon's own name.
                        text: ({ "power-saver": i18nc("@option", "Power Saver"),
                                 "balanced": i18nc("@option", "Balanced"),
                                 "performance": i18nc("@option", "Performance") })[modelData] ?? modelData
                        onClicked: power.profile = modelData
                    }
                }
            }
            QQC2.Label {
                Kirigami.FormData.label: i18nc("@label", "Used while open:")
                text: i18nc("@info energy", "%1 Wh", power.sessionWattHours.toFixed(power.sessionWattHours < 10 ? 2 : 1))
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            Layout.bottomMargin: Kirigami.Units.largeSpacing
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            opacity: 0.6
            text: i18nc("@info", "Measured by the processor (RAPL) and the NVIDIA driver. Screens, drives, fans and the power supply's own losses are not included, so the wall socket shows more.")
        }
    }
}
