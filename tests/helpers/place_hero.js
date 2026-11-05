// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The final screenshot's arrangement: Text Editor behind at the top right, Konsole below it, and the
// File Explorer in front on the left.
let explorer = null;
for (const window of workspace.windowList()) {
    if (!window.normalWindow) {
        continue;
    }
    const name = (window.resourceClass + " " + window.desktopFileName).toLowerCase();
    print("hero window: " + name);
    if (name.includes("texteditor") || name.includes("text-editor")) {
        window.frameGeometry = {x: 1390, y: 72, width: 1060, height: 600};
    } else if (name.includes("konsole")) {
        window.frameGeometry = {x: 1470, y: 690, width: 990, height: 600};
    } else if (name.includes("file-explorer") || name.includes("fileexplorer")) {
        window.frameGeometry = {x: 110, y: 118, width: 1320, height: 830};
        explorer = window;
    }
}
if (explorer) {
    workspace.activeWindow = explorer;
}
