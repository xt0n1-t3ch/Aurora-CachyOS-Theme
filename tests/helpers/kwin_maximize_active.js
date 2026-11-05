// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Maximizes every normal window, so a dodge-windows dock has something to dodge.
for (const w of workspace.windowList()) {
    if (w.normalWindow) {
        w.setMaximize(true, true);
    }
}
