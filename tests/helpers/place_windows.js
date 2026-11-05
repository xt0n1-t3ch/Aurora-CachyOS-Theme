// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Puts the preview windows side by side, by app, and focuses the GTK one, for the sandbox screenshots.
const spots = {
    "org.gnome.TextEditor": {x: 150, y: 170, width: 1060, height: 720},
    "gnome-text-editor": {x: 150, y: 170, width: 1060, height: 720},
    "org.kde.konsole": {x: 1290, y: 260, width: 1120, height: 700},
    "konsole": {x: 1290, y: 260, width: 1120, height: 700},
};
let editor = null;
for (const window of workspace.windowList()) {
    const spot = spots[window.resourceClass] || spots[window.desktopFileName];
    if (spot && window.normalWindow) {
        window.frameGeometry = spot;
        if (window.resourceClass.includes("TextEditor") || window.resourceClass.includes("text-editor")) {
            editor = window;
        }
    }
}
if (editor) {
    workspace.activeWindow = editor;
}
