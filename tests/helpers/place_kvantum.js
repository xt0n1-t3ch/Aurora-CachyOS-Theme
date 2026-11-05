// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Places System Settings on the left and Konsole on the right, for the sandbox screenshots.
for (const window of workspace.windowList()) {
    if (!window.normalWindow) continue;
    const name = (window.resourceClass + " " + window.desktopFileName).toLowerCase();
    if (name.includes("systemsettings")) window.frameGeometry = {x: 90, y: 90, width: 1180, height: 760};
    else if (name.includes("konsole")) window.frameGeometry = {x: 1330, y: 160, width: 1100, height: 660};
}
