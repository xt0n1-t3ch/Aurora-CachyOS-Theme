// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The active app's name in bold next to the menu bar's logo, as on macOS. It takes no room when no window has the
// focus.
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.taskmanager as TaskManager
import "apptitle.js" as AppTitle

PlasmoidItem {
    id: root

    // The windows of this screen and desktop; activeTask is the one with the focus.
    TaskManager.TasksModel {
        id: tasks
        filterByScreen: true
        filterByVirtualDesktop: true
        filterByActivity: true
        screenGeometry: Plasmoid.containment.screenGeometry
        activity: activityInfo.currentActivity
        virtualDesktop: desktopInfo.currentDesktop
    }
    TaskManager.ActivityInfo {
        id: activityInfo
    }
    TaskManager.VirtualDesktopInfo {
        id: desktopInfo
    }

    // Read again whenever the focus moves or the active window's data changes.
    property int revision: 0
    Connections {
        target: tasks
        function onActiveTaskChanged() { root.revision++ }
        function onDataChanged() { root.revision++ }
        function onCountChanged() { root.revision++ }
    }
    readonly property string text: {
        root.revision
        const index = tasks.activeTask
        return AppTitle.label({
            active: index.valid && tasks.data(index, TaskManager.AbstractTasksModel.IsActive) === true,
            appName: index.valid ? tasks.data(index, TaskManager.AbstractTasksModel.AppName) : "",
            title: index.valid ? tasks.data(index, Qt.DisplayRole) : ""
        })
    }

    preferredRepresentation: fullRepresentation
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground

    fullRepresentation: PlasmaComponents.Label {
        Layout.minimumWidth: root.text ? implicitWidth : 0
        Layout.preferredWidth: Layout.minimumWidth
        Layout.maximumWidth: Kirigami.Units.gridUnit * 20
        Layout.fillHeight: true
        leftPadding: root.text ? 4 : 0
        rightPadding: root.text ? 8 : 0
        visible: root.text !== ""
        text: root.text
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        font.weight: Font.Bold
        font.pointSize: 10
        textFormat: Text.PlainText
    }
}
