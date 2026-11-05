// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The menu bar clock of Aurora: "mié 4 nov  8:01 a. m." on one line with macOS's gap between date and time,
// and Plasma's month calendar when clicked.
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.workspace.calendar as PlasmaCalendar
import "clock.js" as Clock

PlasmoidItem {
    id: root

    // The current time, refreshed every second by the timer below.
    property date now: new Date()
    // The locale's day and month names, am/pm texts and hour format, in the shape clock.js takes.
    readonly property var names: {
        const locale = Qt.locale()
        const days = []
        const months = []
        for (let i = 0; i < 7; ++i) {
            days.push(locale.dayName(i, Locale.ShortFormat))
        }
        for (let i = 0; i < 12; ++i) {
            months.push(locale.monthName(i, Locale.ShortFormat))
        }
        // Qt takes the names from LC_TIME but its name from LANG, so the language shows in the names themselves:
        // English puts the month first ("Wed Nov 4"), the others the day ("mié 4 nov").
        const english = days[3] === Qt.locale("en_US").dayName(3, Locale.ShortFormat)
        const regionTime = locale.timeFormat(Locale.ShortFormat)
        return {
            days: days,
            months: months,
            am: locale.amText,
            pm: locale.pmText,
            dayFirst: !english,
            uses24h: Clock.uses24h(Plasmoid.configuration.use24hFormat, regionTime),
            padHours: /HH/.test(regionTime)
        }
    }
    readonly property string dateText: Clock.dateText(now, names)
    readonly property string timeText: Clock.timeText(now, names)

    // The clock is always the bar item; the calendar opens in Plasma's popup.
    preferredRepresentation: compactRepresentation
    toolTipMainText: now.toLocaleDateString(Qt.locale(), Locale.LongFormat)
    toolTipSubText: ""

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: root.now = new Date()
    }

    compactRepresentation: MouseArea {
        id: compact

        Layout.minimumWidth: row.implicitWidth + 16
        Layout.preferredWidth: row.implicitWidth + 16
        hoverEnabled: true
        onClicked: root.expanded = !root.expanded

        // Like macOS, a capsule marks the clock while its panel is open.
        Rectangle {
            anchors.centerIn: parent
            width: row.implicitWidth + 16
            height: Math.min(parent.height - 6, 24)
            radius: height / 2
            color: "white"
            opacity: root.expanded ? 0.20 : compact.containsMouse ? 0.10 : 0

            Behavior on opacity {
                NumberAnimation { duration: 120 }
            }
        }

        Row {
            id: row
            anchors.centerIn: parent
            // macOS leaves about two spaces between the date and the time.
            spacing: Math.round(Kirigami.Theme.defaultFont.pixelSize * 0.6)

            Text {
                visible: Plasmoid.configuration.showDate
                text: root.dateText
                color: Kirigami.Theme.textColor
                font: Kirigami.Theme.defaultFont
            }

            Text {
                text: root.timeText
                color: Kirigami.Theme.textColor
                font: Kirigami.Theme.defaultFont
            }
        }
    }

    // The month calendar, without Plasma's digital clock header.
    fullRepresentation: Item {
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        Layout.preferredHeight: Kirigami.Units.gridUnit * 21
        Layout.minimumWidth: Kirigami.Units.gridUnit * 18
        Layout.minimumHeight: Kirigami.Units.gridUnit * 19

        PlasmaCalendar.MonthView {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing
            borderOpacity: 0.25
            today: root.now
            firstDayOfWeek: Qt.locale().firstDayOfWeek
            showDigitalClockHeader: false
        }
    }
}
