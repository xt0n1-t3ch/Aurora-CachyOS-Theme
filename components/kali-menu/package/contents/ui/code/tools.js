/*
    SPDX-FileCopyrightText: 2013 Aurélien Gâteau <agateau@kde.org>
    SPDX-FileCopyrightText: 2013-2015 Eike Hein <hein@kde.org>
    SPDX-FileCopyrightText: 2017 Ivan Cukic <ivan.cukic@kde.org>
    SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>

    SPDX-License-Identifier: GPL-2.0-or-later

    Begin: code from plasma-desktop/applets/kickoff/contents/ui/code/tool.js
*/

// Aurora: the menu bar shows CachyOS's black-and-white logo unless the user picks another one (Kickoff defaults to
// "start-here-kde-symbolic"). See docs/decisions.md, Menu bar logo.
const defaultIconName = "aurora-logo-cachyos-symbolic";

function iconOrDefault(formFactor, preferredIconName) {
    // Vertical panels must have an icon, at least a default one.
    return (formFactor === PlasmaCore.Types.Vertical && preferredIconName === "")
        ? defaultIconName : preferredIconName;
}
/*
 * End: code from plasma-desktop/applets/kickoff/contents/ui/code/tool.js
*/

// Aurora: the logos the settings page offers for the menu bar, in the order it shows them. "white" is the symbolic
// icon, drawn in the menu bar's text color; "colored" is the logo in its own colors, when it has them
// (themes/icons/menu_logos.py lists where each comes from).
const logos = [
    {id: "cachyos", name: "CachyOS", white: "aurora-logo-cachyos-symbolic", colored: "aurora-logo-cachyos"},
    {id: "arch", name: "Arch Linux", white: "aurora-logo-arch-symbolic", colored: "aurora-logo-arch"},
    {id: "apple", name: "Apple", white: "aurora-logo-apple-symbolic", colored: ""},
    {id: "kali", name: "Kali Linux", white: "aurora-logo-kali-symbolic", colored: ""},
];

// Whether the logo also comes in its own colors.
function hasColors(id) {
    const logo = logos.find(logo => logo.id === id);
    return !!logo && logo.colored !== "";
}

// The icon name to save for a logo; a logo without colors gives its white icon either way.
function logoIcon(id, colored) {
    const logo = logos.find(logo => logo.id === id);
    if (!logo) {
        return "";
    }
    return colored && logo.colored !== "" ? logo.colored : logo.white;
}

// The logo a saved icon name shows, or null for an icon the user picked themselves.
function logoOf(icon) {
    for (const logo of logos) {
        if (icon === logo.white || (icon === logo.colored && icon !== "")) {
            return {id: logo.id, colored: icon === logo.colored};
        }
    }
    return null;
}
