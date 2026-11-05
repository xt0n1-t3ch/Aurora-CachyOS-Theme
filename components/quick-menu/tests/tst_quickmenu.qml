// SPDX-License-Identifier: GPL-2.0-or-later
// Quick Menu's Applications view logic (vendor/krunner/qml/quickmenu.js): how Plasma's menu becomes Tahoe's sections,
// what one row shows, and where the arrow keys go. Run with: qmltestrunner -input tests/tst_quickmenu.qml
import QtQuick
import QtTest
import "../vendor/krunner/qml/quickmenu.js" as QuickMenu

TestCase {
    name: "Quick Menu"

    function app(name, id) {
        return {id: id || "applications:" + name.toLowerCase() + ".desktop", name: name}
    }
    function titles(sections) {
        return sections.map(section => section.title)
    }
    function names(section) {
        return section.apps.map(entry => entry.name)
    }

    function test_plasma_categories_join_the_tahoe_section_they_belong_to() {
        const sections = QuickMenu.buildSections([
            {icon: "applications-system-symbolic", name: "System", apps: [app("Konsole")]},
            {icon: "applications-utilities-symbolic", name: "Utilities", apps: [app("Ark")]},
            {icon: "help-about-symbolic", name: "Help", apps: [app("Info Center")]},
            {icon: "applications-development-symbolic", name: "Development", apps: [app("Kate")]},
        ])
        compare(titles(sections), ["Developer Tools", "Utilities"])
        compare(names(sections[1]), ["Ark", "Info Center", "Konsole"])
    }

    function test_an_app_listed_in_two_categories_shows_once_in_their_section() {
        const kate = app("Kate", "applications:org.kde.kate.desktop")
        const sections = QuickMenu.buildSections([
            {icon: "applications-system", name: "System", apps: [kate, app("Konsole")]},
            {icon: "applications-utilities", name: "Utilities", apps: [kate]},
        ])
        compare(names(sections[0]), ["Kate", "Konsole"])
    }

    function test_apps_are_listed_alphabetically_whatever_their_case() {
        const sections = QuickMenu.buildSections([
            {icon: "applications-games", name: "Games", apps: [app("Steam"), app("btop++"), app("Lutris"), app("Faugus")]},
        ])
        compare(names(sections[0]), ["btop++", "Faugus", "Lutris", "Steam"])
    }

    function test_sections_follow_tahoe_order_and_a_menu_of_its_own_keeps_its_name_before_other() {
        const sections = QuickMenu.buildSections([
            {icon: "applications-other", name: "Lost & Found", apps: [app("Stray")]},
            {icon: "applications-utilities", name: "Utilities", apps: [app("Ark")]},
            {icon: "wine", name: "Wine", apps: [app("Notepad")]},
            {icon: "applications-internet", name: "Internet", apps: [app("Discord")]},
            {icon: "applications-office", name: "Office", apps: [app("Writer")]},
        ])
        compare(titles(sections), ["Productivity & Finance", "Internet", "Utilities", "Wine", "Other"])
    }

    function test_a_category_without_apps_is_left_out() {
        const sections = QuickMenu.buildSections([
            {icon: "applications-education", name: "Education", apps: []},
            {icon: "applications-graphics", name: "Graphics", apps: [app("Gwenview")]},
        ])
        compare(titles(sections), ["Creativity"])
    }

    function test_all_applications_lists_every_app_once_under_its_first_letter() {
        const kate = app("Kate", "applications:org.kde.kate.desktop")
        const sections = QuickMenu.alphabeticalSections([
            {icon: "applications-system", name: "System", apps: [kate, app("konsole"), app("Ark")]},
            {icon: "applications-utilities", name: "Utilities", apps: [kate, app("Ébano"), app("Discord")]},
        ])
        compare(titles(sections), ["A", "D", "E", "K"])
        compare(names(sections[3]), ["Kate", "konsole"])
        // An accent files a name under its plain letter, as Windows' All apps list does.
        compare(names(sections[2]), ["Ébano"])
    }

    function test_names_that_start_with_a_digit_or_a_sign_come_first_under_a_hash() {
        const sections = QuickMenu.alphabeticalSections([
            {icon: "applications-other", name: "Other", apps: [app("Zoom"), app("7-Zip"), app("&Notes")]},
        ])
        compare(titles(sections), ["#", "Z"])
        compare(names(sections[0]), ["&Notes", "7-Zip"])
    }

    function test_all_applications_shows_every_app_without_show_more() {
        const many = []
        for (let i = 0; i < 12; ++i) {
            many.push(app("App " + i))
        }
        const section = QuickMenu.alphabeticalSections([{icon: "applications-other", name: "Other", apps: many}])[0]
        compare(QuickMenu.visibleApps(section, false, 5).length, 12)
        verify(!QuickMenu.canExpand(section, 5))
    }

    function test_a_section_shows_one_row_until_show_more() {
        const apps = ["A", "B", "C", "D", "E", "F", "G"].map(name => app(name))
        const section = {title: "Utilities", apps: apps}
        compare(QuickMenu.visibleApps(section, false, 5).length, 5)
        compare(QuickMenu.visibleApps(section, true, 5).length, 7)
        verify(QuickMenu.canExpand(section, 5))
        verify(!QuickMenu.canExpand({title: "Games", apps: apps.slice(0, 5)}, 5))
    }

    // Two sections of a 5-column grid: Suggestions shows 5 apps, Utilities 7 (two rows: 5 + 2).
    readonly property var grid: [5, 7]

    function test_right_moves_to_the_next_app_and_on_into_the_next_section() {
        compare(QuickMenu.move(grid, {section: 0, index: 3}, "right", 5), {section: 0, index: 4})
        compare(QuickMenu.move(grid, {section: 0, index: 4}, "right", 5), {section: 1, index: 0})
        compare(QuickMenu.move(grid, {section: 1, index: 6}, "right", 5), {section: 1, index: 6})
    }

    function test_left_moves_back_into_the_previous_section() {
        compare(QuickMenu.move(grid, {section: 1, index: 0}, "left", 5), {section: 0, index: 4})
        compare(QuickMenu.move(grid, {section: 0, index: 0}, "left", 5), {section: 0, index: 0})
    }

    function test_down_keeps_the_column_and_stops_at_the_last_app_of_a_short_row() {
        compare(QuickMenu.move(grid, {section: 0, index: 2}, "down", 5), {section: 1, index: 2})
        compare(QuickMenu.move(grid, {section: 1, index: 4}, "down", 5), {section: 1, index: 6})
        compare(QuickMenu.move(grid, {section: 1, index: 6}, "down", 5), {section: 1, index: 6})
    }

    function test_up_keeps_the_column_and_leaves_the_grid_from_its_first_row() {
        compare(QuickMenu.move(grid, {section: 1, index: 6}, "up", 5), {section: 1, index: 1})
        compare(QuickMenu.move(grid, {section: 1, index: 3}, "up", 5), {section: 0, index: 3})
        compare(QuickMenu.move(grid, {section: 0, index: 3}, "up", 5), null)
    }

    function test_down_from_the_search_field_enters_the_first_app() {
        compare(QuickMenu.move(grid, null, "down", 5), {section: 0, index: 0})
        compare(QuickMenu.move([], null, "down", 5), null)
    }

    // Dragging an app within its section: the saved order is a list of app ids per section title.
    readonly property var devCategory: ({icon: "applications-development", name: "Development",
                                         apps: [app("CMake"), app("Kate"), app("Meld"), app("Micro"), app("Orion")]})

    function test_dragging_an_app_forward_puts_it_where_it_was_dropped() {
        compare(QuickMenu.reorder(["a", "b", "c", "d"], 0, 2), ["b", "c", "a", "d"])
    }

    function test_dragging_an_app_back_puts_it_before_the_app_it_was_dropped_on() {
        compare(QuickMenu.reorder(["a", "b", "c", "d", "e"], 4, 0), ["e", "a", "b", "c", "d"])
        compare(QuickMenu.reorder(["a", "b", "c"], 2, 1), ["a", "c", "b"])
    }

    function test_dropping_an_app_on_itself_or_outside_the_list_changes_nothing() {
        compare(QuickMenu.reorder(["a", "b", "c"], 1, 1), ["a", "b", "c"])
        compare(QuickMenu.reorder(["a", "b", "c"], 5, 0), ["a", "b", "c"])
        compare(QuickMenu.reorder(["a", "b", "c"], 0, -1), ["a", "b", "c"])
    }

    function test_a_saved_order_is_how_the_section_lists_its_apps() {
        const orion = "applications:orion.desktop"
        const sections = QuickMenu.buildSections([devCategory], {"Developer Tools": [orion, "applications:cmake.desktop"]})
        compare(names(sections[0]), ["Orion", "CMake", "Kate", "Meld", "Micro"])
    }

    function test_an_app_installed_after_the_order_was_saved_follows_the_ordered_ones_alphabetically() {
        const saved = {"Developer Tools": ["applications:micro.desktop", "applications:kate.desktop"]}
        const sections = QuickMenu.buildSections([devCategory], saved)
        compare(names(sections[0]), ["Micro", "Kate", "CMake", "Meld", "Orion"])
    }

    function test_an_uninstalled_app_in_a_saved_order_is_skipped() {
        const saved = {"Developer Tools": ["applications:gone.desktop", "applications:meld.desktop"]}
        compare(names(QuickMenu.buildSections([devCategory], saved)[0]), ["Meld", "CMake", "Kate", "Micro", "Orion"])
    }

    function test_an_order_saved_for_one_section_leaves_the_others_alphabetical() {
        const sections = QuickMenu.buildSections([devCategory, {icon: "applications-games", name: "Games",
                                                                apps: [app("Steam"), app("Lutris")]}],
                                                 {"Developer Tools": ["applications:orion.desktop"]})
        compare(names(sections[0]), ["Lutris", "Steam"])
        compare(names(sections[1])[0], "Orion")
    }

    // The right-click menu of an app: pin it to the top of Quick Menu, pin it to the Dock, remove it from Suggestions,
    // and Plasma's own actions for it (Kicker's ActionListRole: jump list items, Edit Application, Uninstall).
    readonly property var jump: ({text: "New Window", icon: "window-new", actionId: "_kicker_jumpListAction"})
    readonly property var edit: ({text: "Edit Application…", icon: "kmenuedit", actionId: "editApplication"})
    readonly property var uninstall: ({text: "Uninstall or Manage Add-Ons…", icon: "plasmadiscover",
                                        actionId: "manageApplication"})
    readonly property var separator: ({type: "separator"})

    function labels(items) {
        return items.map(item => item.separator ? "-" : item.label)
    }

    function test_pinned_apps_get_their_own_section_in_pin_order() {
        const pins = ["applications:orion.desktop", "applications:kate.desktop"]
        const section = QuickMenu.pinnedSection([devCategory], pins)
        compare(section.title, "Pinned")
        compare(names(section), ["Orion", "Kate"])
        verify(section.pinned)
    }

    function test_a_pinned_app_that_is_gone_is_left_out_and_no_pins_means_no_section() {
        const pins = ["applications:gone.desktop", "applications:meld.desktop"]
        compare(names(QuickMenu.pinnedSection([devCategory], pins)), ["Meld"])
        compare(QuickMenu.pinnedSection([devCategory], []), null)
        compare(QuickMenu.pinnedSection([devCategory], ["applications:gone.desktop"]), null)
    }

    function test_pinning_adds_an_app_at_the_end_and_unpinning_removes_it() {
        compare(QuickMenu.togglePin(["a"], "b"), ["a", "b"])
        compare(QuickMenu.togglePin(["a", "b"], "a"), ["b"])
    }

    function test_the_menu_of_an_app_lists_open_its_actions_the_pins_and_plasma_s_actions() {
        const items = QuickMenu.menuItems({suggested: false, pinned: false, inDock: false,
                                           actions: [jump, separator, edit, uninstall]})
        compare(labels(items), ["Open", "New Window", "-", "Pin to Top", "Pin to Dock", "-",
                                "Edit Application…", "Uninstall or Manage Add-Ons…"])
        compare(items[0].action, {kind: "open"})
        compare(items[1].action, {kind: "plasma", action: jump})
    }

    function test_a_pinned_suggested_app_in_the_dock_offers_to_undo_each() {
        const items = QuickMenu.menuItems({suggested: true, pinned: true, inDock: true, actions: []})
        compare(labels(items), ["Open", "-", "Unpin from Top", "Unpin from Dock", "Remove from Suggestions"])
        compare(items[2].action, {kind: "pin"})
        compare(items[3].action, {kind: "dock"})
        compare(items[4].action, {kind: "forget"})
    }

    function test_the_menu_never_shows_two_separators_in_a_row_or_at_an_end() {
        const items = QuickMenu.menuItems({suggested: false, pinned: false, inDock: false,
                                           actions: [separator, separator, edit, separator]})
        compare(labels(items), ["Open", "-", "Pin to Top", "Pin to Dock", "-", "Edit Application…"])
    }

    function test_every_entry_has_a_symbolic_icon_like_tahoe_s_menus() {
        const items = QuickMenu.menuItems({suggested: true, pinned: false, inDock: true,
                                           actions: [jump, separator, edit, uninstall]})
        compare(items.filter(item => !item.separator).map(item => item.icon),
                ["external-link-symbolic", "window-new-symbolic", "view-pin-symbolic", "dock-symbolic",
                 "view-hidden-symbolic", "document-edit-symbolic", "edit-delete-symbolic"])
        compare(QuickMenu.menuItems({suggested: false, pinned: true, inDock: false, actions: []})[2].icon,
                "window-unpin-symbolic")
    }

    function test_a_jump_list_item_without_a_symbolic_icon_falls_back_to_a_new_window() {
        const items = QuickMenu.menuItems({suggested: false, pinned: false, inDock: false,
                                           actions: [{text: "New Private Window", icon: "firefox",
                                                      actionId: "_kicker_jumpListAction"}]})
        compare(items[1].icon, "firefox-symbolic")
        compare(items[1].fallback, "window-new-symbolic")
    }

    // What Kicker adds for an app under Suggestions (RecentUsageModel): its recent files under a title, and its own
    // forget actions.
    readonly property var recentTitle: ({type: "title", text: "Recent Files"})
    readonly property var recentFile: ({text: "notes.png", icon: "image-png", actionId: "_kicker_recentDocument"})
    readonly property var forgetApp: ({text: "Forget Application", icon: "edit-clear-history", actionId: "forget"})
    readonly property var forgetAll: ({text: "Forget All Applications", icon: "edit-clear-history",
                                        actionId: "forgetAll"})

    function test_kicker_s_forget_actions_are_left_to_remove_from_suggestions() {
        const items = QuickMenu.menuItems({suggested: true, pinned: false, inDock: false,
                                           actions: [forgetApp, forgetAll]})
        compare(labels(items), ["Open", "-", "Pin to Top", "Pin to Dock", "Remove from Suggestions"])
    }

    function test_a_title_stays_a_title_and_recent_files_get_a_document_icon() {
        const items = QuickMenu.menuItems({suggested: true, pinned: false, inDock: false,
                                           actions: [recentTitle, recentFile]})
        const title = items.find(item => item.label === "Recent Files")
        verify(title.title === true && title.action === undefined)
        const file = items.find(item => item.label === "notes.png")
        compare(file.icon, "image-png-symbolic")
        compare(file.fallback, "text-x-generic-symbolic")
    }

    function test_plasma_s_own_pin_actions_are_left_to_aurora_s() {
        const pinToPanel = {text: "Pin to Task Manager", actionId: "addToTaskManager"}
        compare(labels(QuickMenu.menuItems({suggested: false, pinned: false, inDock: false, actions: [pinToPanel]})),
                ["Open", "-", "Pin to Top", "Pin to Dock"])
    }

    function test_the_dock_launchers_are_read_from_the_shell_s_answer() {
        const launchers = QuickMenu.launchers("applications:a.desktop,applications:b.desktop|applications:c.desktop")
        verify(launchers.indexOf("applications:b.desktop") >= 0 && launchers.indexOf("applications:c.desktop") >= 0)
        compare(QuickMenu.launchers(""), [])
    }

    function test_the_dock_script_adds_or_removes_one_launcher_on_every_dock() {
        const add = QuickMenu.dockScript("applications:org.kde.kate.desktop", true)
        verify(add.indexOf("widgets('org.kde.plasma.icontasks')") >= 0)
        verify(add.indexOf("'applications:org.kde.kate.desktop'") >= 0)
        verify(add.indexOf("push") >= 0)
        verify(QuickMenu.dockScript("applications:org.kde.kate.desktop", false).indexOf("filter") >= 0)
    }

    function test_the_dock_script_refuses_an_id_that_is_not_a_desktop_file() {
        compare(QuickMenu.dockScript("applications:x'); evil(); ('.desktop", true), "")
        compare(QuickMenu.dockScript("file:///etc/passwd", true), "")
    }

    // The power rail (QuickMenuRail.qml): each menu lists only what the session allows, and each entry asks Plasma's
    // session manager for its own action.
    function railLabels(items) {
        return items.map(item => item.label)
    }

    function test_the_account_menu_lists_what_the_session_allows() {
        compare(railLabels(QuickMenu.accountItems({canLock: true, canSwitchUser: true, canLogout: true})),
                ["Lock", "Switch User", "Log Out"])
        compare(railLabels(QuickMenu.accountItems({canLock: true, canSwitchUser: false, canLogout: true})), ["Lock", "Log Out"])
    }

    function test_the_power_menu_lists_hibernate_only_where_the_machine_can() {
        const all = {canSuspend: true, canHibernate: true, canReboot: true, canShutdown: true}
        compare(railLabels(QuickMenu.powerItems(all)), ["Sleep", "Hibernate", "Restart", "Shut Down"])
        compare(railLabels(QuickMenu.powerItems(Object.assign({}, all, {canHibernate: false}))), ["Sleep", "Restart", "Shut Down"])
        compare(railLabels(QuickMenu.powerItems({})), [])
    }

    function test_each_entry_calls_its_own_session_action() {
        const calls = []
        const session = {}
        for (const name of ["lock", "switchUser", "requestLogout", "suspend", "hibernate", "requestReboot", "requestShutdown"]) {
            session[name] = () => calls.push(name)
        }
        const everything = {canLock: true, canSwitchUser: true, canLogout: true, canSuspend: true, canHibernate: true,
                            canReboot: true, canShutdown: true}
        for (const item of QuickMenu.accountItems(everything).concat(QuickMenu.powerItems(everything))) {
            verify(QuickMenu.runSessionAction(session, item.action))
        }
        compare(calls, ["lock", "switchUser", "requestLogout", "suspend", "hibernate", "requestReboot", "requestShutdown"])
    }

    // Spotlight drops in: a fade, a small scale that slows down at the end and a short fall from above, all scaled by
    // the desktop's animation speed through Kirigami's durations; at speed 0 nothing moves.
    function test_the_panel_drops_in_like_spotlight() {
        const motion = QuickMenu.openingMotion(200)
        compare(motion.scaleFrom, 0.96)
        compare(motion.liftFrom, -10)
        compare(motion.fadeDuration, 200)
        compare(motion.moveDuration, 280)
    }

    function test_instant_animations_open_the_panel_at_once() {
        const motion = QuickMenu.openingMotion(0)
        compare(motion.fadeDuration, 0)
        compare(motion.moveDuration, 0)
    }

    function test_an_unknown_entry_calls_nothing() {
        let called = false
        compare(QuickMenu.runSessionAction({lock: () => called = true}, "format-disk"), false)
        verify(!called)
    }
}
