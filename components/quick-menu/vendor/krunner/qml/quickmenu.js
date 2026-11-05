/*
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: GPL-2.0-or-later
*/
// Quick Menu's Applications view as macOS Tahoe draws it: Suggestions first, then the apps by section, one row of
// each until "Show More". The catalog is Plasma's own menu (Kicker's RootModel); a menu category is keyed by its
// icon from /usr/share/desktop-directories, which does not change with the language.
.pragma library

// Plasma menu category icon (without "-symbolic") -> the Tahoe section it joins.
const SECTION_OF = {
    "applications-office": "Productivity & Finance",
    "applications-graphics": "Creativity",
    "applications-internet": "Internet",
    "applications-multimedia": "Entertainment",
    "applications-games": "Games",
    "applications-development": "Developer Tools",
    "applications-education": "Education",
    "applications-science": "Education",
    "applications-system": "Utilities",
    "applications-utilities": "Utilities",
    "preferences-system": "Utilities",
    "help-about": "Utilities",
    "applications-other": "Other",
};
// Tahoe's own order; a menu of its own (a "Wine" folder) keeps its name and comes just before Other.
const ORDER = ["Productivity & Finance", "Creativity", "Internet", "Entertainment", "Games", "Developer Tools",
               "Education", "Utilities"];
const LAST = "Other";

function sectionTitle(icon, name) {
    return SECTION_OF[String(icon).replace(/-symbolic$/, "")] || name;
}

function rank(title) {
    const known = ORDER.indexOf(title);
    if (known >= 0) {
        return [0, known, ""];
    }
    return title === LAST ? [2, 0, ""] : [1, 0, title.toLocaleLowerCase()];
}

function compareRanks(a, b) {
    for (let i = 0; i < a.length; ++i) {
        if (a[i] !== b[i]) {
            return a[i] < b[i] ? -1 : 1;
        }
    }
    return 0;
}

// order: the app ids of a section in the order the user dragged them into. Apps it names come first, in that
// order; apps it does not name (installed since) follow in their own order; ids of apps that are gone are skipped.
function applyOrder(apps, order) {
    if (!order || order.length === 0) {
        return apps;
    }
    const position = new Map(order.map((id, i) => [id, i]));
    const named = apps.filter(entry => position.has(entry.id)).sort((a, b) => position.get(a.id) - position.get(b.id));
    return named.concat(apps.filter(entry => !position.has(entry.id)));
}

// ids with the one at `from` moved to `to`, the place of the app it was dropped on; a move outside the list or onto
// itself changes nothing.
function reorder(ids, from, to) {
    if (from === to || from < 0 || to < 0 || from >= ids.length || to >= ids.length) {
        return ids.slice();
    }
    const next = ids.slice();
    const moved = next.splice(from, 1)[0];
    next.splice(to, 0, moved);
    return next;
}

// categories: [{icon, name, apps: [{id, name, ...}]}] -> [{title, apps, reorderable}], merged, without duplicates,
// alphabetical unless `orders` ({section title: [app ids]}) holds an order the user dragged the apps into.
function buildSections(categories, orders) {
    const byTitle = new Map();
    for (const category of categories) {
        const title = sectionTitle(category.icon, category.name);
        let section = byTitle.get(title);
        if (!section) {
            section = {title: title, apps: [], ids: new Set()};
            byTitle.set(title, section);
        }
        for (const entry of category.apps) {
            if (!section.ids.has(entry.id)) {
                section.ids.add(entry.id);
                section.apps.push(entry);
            }
        }
    }
    const sections = [...byTitle.values()].filter(section => section.apps.length > 0);
    for (const section of sections) {
        section.apps.sort((a, b) => a.name.localeCompare(b.name, undefined, {sensitivity: "base"}));
        section.apps = applyOrder(section.apps, (orders || {})[section.title]);
        section.reorderable = true;
        delete section.ids;
    }
    return sections.sort((a, b) => compareRanks(rank(a.title), rank(b.title)));
}

// The apps pinned with "Pin to Top", in pin order, as the first section of Quick Menu; null when none of them is
// installed. Like Windows' pinned apps, they stay there whatever their category or how often they are used.
function pinnedSection(categories, pins) {
    const byId = new Map();
    for (const category of categories) {
        for (const entry of category.apps) {
            if (!byId.has(entry.id)) {
                byId.set(entry.id, entry);
            }
        }
    }
    const apps = (pins || []).filter(id => byId.has(id)).map(id => byId.get(id));
    return apps.length > 0 ? {title: "Pinned", apps: apps, pinned: true, reorderable: true} : null;
}

// The pin list with `id` pinned at the end, or unpinned when it was there.
function togglePin(pins, id) {
    return pins.indexOf(id) >= 0 ? pins.filter(pin => pin !== id) : pins.concat([id]);
}

// Plasma's actions that Aurora leaves out: its pins (Kicker only offers them inside a panel widget) and Kicker's
// forget actions, which "Remove from Suggestions" replaces; "Forget All Applications" would empty Suggestions from
// one app's menu.
const REPLACED_ACTIONS = ["addToTaskManager", "addToDesktop", "addToPanel", "hideApplication", "forget", "forgetAll"];

// Symbolic icons for Plasma's own actions, by action id; any other action gets its icon's symbolic version.
const ACTION_ICONS = {editApplication: "document-edit-symbolic", manageApplication: "edit-delete-symbolic"};
// The icon shown when an action's own symbolic icon does not exist, by action id.
const FALLBACK_ICONS = {
    _kicker_jumpListAction: "window-new-symbolic",
    _kicker_recentDocument: "text-x-generic-symbolic",
};

// The right-click menu of an app, top to bottom. `app` says whether the app is in Suggestions, pinned and in the
// Dock, and holds Plasma's actions for it (Kicker's ActionListRole: jump list items, Edit Application, Uninstall).
// Each item is {label, icon, fallback, action}, {title: true, label} for a heading such as Kicker's "Recent Files", or
// {separator: true}; icons are symbolic, as in macOS Tahoe's menus, and there is never a separator at an end or two
// in a row.
function menuItems(app) {
    const plasma = (app.actions || []).filter(action => REPLACED_ACTIONS.indexOf(action.actionId) < 0);
    const isJump = action => action.actionId === "_kicker_jumpListAction";
    const entry = (label, icon, action, fallback) =>
        ({label: label, icon: icon, fallback: fallback || "", action: action});
    const plasmaItem = action => {
        if (action.type === "separator") {
            return {separator: true};
        }
        if (action.type === "title") {
            return {title: true, label: action.text};
        }
        const icon = ACTION_ICONS[action.actionId] || (action.icon ? action.icon + "-symbolic" : "");
        return entry(action.text, icon, {kind: "plasma", action: action}, FALLBACK_ICONS[action.actionId]);
    };
    const items = [entry("Open", "external-link-symbolic", {kind: "open"})];
    plasma.filter(action => action.type !== "separator" && isJump(action))
        .forEach(action => items.push(plasmaItem(action)));
    items.push({separator: true});
    items.push(app.pinned ? entry("Unpin from Top", "window-unpin-symbolic", {kind: "pin"})
                          : entry("Pin to Top", "view-pin-symbolic", {kind: "pin"}));
    items.push(entry(app.inDock ? "Unpin from Dock" : "Pin to Dock", "dock-symbolic", {kind: "dock"}));
    if (app.suggested) {
        items.push(entry("Remove from Suggestions", "view-hidden-symbolic", {kind: "forget"}));
    }
    items.push({separator: true});
    plasma.filter(action => !isJump(action)).forEach(action => items.push(plasmaItem(action)));
    return items.filter((item, i, all) => !item.separator
                        || (i > 0 && !all[i - 1].separator && all.slice(i + 1).some(next => !next.separator)));
}

// The Dock's launchers, from the shell's answer to the reading script (several Docks are separated by "|").
function launchers(text) {
    return String(text || "").split(/[|,]/).map(part => part.trim()).filter(part => part.length > 0);
}

// The Plasma script that pins `id` to every Dock (at its end) or unpins it, for org.kde.PlasmaShell.evaluateScript.
// Empty when `id` is not a desktop file id, so nothing but a launcher name can reach the script.
function dockScript(id, pin) {
    if (!/^applications:[A-Za-z0-9._+-]+\.desktop$/.test(id)) {
        return "";
    }
    return "var id = '" + id + "'; var changed = 0;\n"
        + "panels().forEach(function (p) { p.widgets('org.kde.plasma.icontasks').forEach(function (w) {\n"
        + "  w.currentConfigGroup = ['General'];\n"
        + "  var list = String(w.readConfig('launchers', '')).split(',')\n"
        + "      .filter(function (l) { return l.length > 0 && l !== id; });\n"
        + (pin ? "  list.push(id);\n" : "")
        + "  w.writeConfig('launchers', list.join(',')); changed++; }); });\n"
        + "print(changed);";
}

// The Plasma script that prints every Dock's launchers, for launchers().
function dockReadScript() {
    return "var out = []; panels().forEach(function (p) { p.widgets('org.kde.plasma.icontasks').forEach(function (w) {"
        + " w.currentConfigGroup = ['General']; out.push(String(w.readConfig('launchers', ''))); }); });"
        + " print(out.join('|'));";
}

function visibleApps(section, expanded, columns) {
    return expanded ? section.apps : section.apps.slice(0, columns);
}

function canExpand(section, columns) {
    return section.apps.length > columns;
}

// counts: how many apps each section shows; current: {section, index} or null for the search field.
// Left and right walk the apps in reading order across sections; up and down keep the column, landing on the
// nearest app of a shorter row. Up from the first row leaves the grid (null); the edges otherwise hold.
function move(counts, current, key, columns) {
    const sections = counts.map((count, section) => ({section: section, count: count})).filter(s => s.count > 0);
    if (sections.length === 0) {
        return null;
    }
    if (!current) {
        return key === "down" || key === "right" ? {section: sections[0].section, index: 0} : null;
    }
    const at = sections.findIndex(s => s.section === current.section);
    const here = sections[at];
    const column = current.index % columns;
    const row = Math.floor(current.index / columns);
    const lastRow = Math.floor((here.count - 1) / columns);
    const clamp = (s, r) => ({section: s.section, index: Math.min(r * columns + column, s.count - 1)});

    if (key === "right") {
        if (current.index + 1 < here.count) {
            return {section: here.section, index: current.index + 1};
        }
        return at + 1 < sections.length ? {section: sections[at + 1].section, index: 0} : current;
    }
    if (key === "left") {
        if (current.index > 0) {
            return {section: here.section, index: current.index - 1};
        }
        return at > 0 ? {section: sections[at - 1].section, index: sections[at - 1].count - 1} : current;
    }
    if (key === "down") {
        if (row < lastRow) {
            return clamp(here, row + 1);
        }
        return at + 1 < sections.length ? clamp(sections[at + 1], 0) : current;
    }
    if (key === "up") {
        if (row > 0) {
            return clamp(here, row - 1);
        }
        if (at === 0) {
            return null;
        }
        const previous = sections[at - 1];
        return clamp(previous, Math.floor((previous.count - 1) / columns));
    }
    return current;
}

// The power rail's menus (QuickMenuRail.qml), from what Plasma's session manager allows (its can* properties). Items have
// the menu's {label, icon, fallback, action} form; the action names the entry for runSessionAction().
function accountItems(can) {
    const items = [];
    if (can.canLock) {
        items.push({label: "Lock", icon: "system-lock-screen-symbolic", fallback: "system-lock-screen", action: "lock"});
    }
    if (can.canSwitchUser) {
        items.push({label: "Switch User", icon: "system-switch-user-symbolic", fallback: "system-switch-user", action: "switch-user"});
    }
    if (can.canLogout) {
        items.push({label: "Log Out", icon: "system-log-out-symbolic", fallback: "system-log-out", action: "log-out"});
    }
    return items;
}

function powerItems(can) {
    const items = [];
    if (can.canSuspend) {
        items.push({label: "Sleep", icon: "system-suspend-symbolic", fallback: "system-suspend", action: "sleep"});
    }
    if (can.canHibernate) {
        items.push({label: "Hibernate", icon: "system-suspend-hibernate-symbolic", fallback: "system-suspend-hibernate", action: "hibernate"});
    }
    if (can.canReboot) {
        items.push({label: "Restart", icon: "system-reboot-symbolic", fallback: "system-reboot", action: "restart"});
    }
    if (can.canShutdown) {
        items.push({label: "Shut Down", icon: "system-shutdown-symbolic", fallback: "system-shutdown", action: "shut-down"});
    }
    return items;
}

// The session manager's method for each entry. Log Out, Restart and Shut Down go through Plasma's own prompt when the
// user keeps it on (System Settings > Session > Desktop Session).
const SESSION_METHODS = {"lock": "lock", "switch-user": "switchUser", "log-out": "requestLogout", "sleep": "suspend",
                         "hibernate": "hibernate", "restart": "requestReboot", "shut-down": "requestShutdown"};

// Calls `action`'s method on `session`; false, calling nothing, for an action it does not know.
function runSessionAction(session, action) {
    const method = SESSION_METHODS[action];
    if (!method) {
        return false;
    }
    session[method]();
    return true;
}

// How the panel opens, like Spotlight: a fade over Kirigami's long duration, then a small scale and a fall of 10 px
// that slow down at the end over a little longer. `longDuration` already carries the desktop's animation speed, so
// instant animations (0) open the panel at once.
function openingMotion(longDuration) {
    return {
        scaleFrom: 0.96,
        liftFrom: -10,
        fadeDuration: longDuration,
        moveDuration: Math.round(longDuration * 1.4),
    }
}
