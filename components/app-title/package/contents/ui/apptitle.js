// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// What the menu bar writes for the active window: { active, appName, title } gives its app's name, the window's
// title when the app has no name, and an empty text when no window has the focus.
.pragma library

function label(window) {
    if (!window.active) {
        return ""
    }
    return (window.appName || "").trim() || (window.title || "").trim()
}
