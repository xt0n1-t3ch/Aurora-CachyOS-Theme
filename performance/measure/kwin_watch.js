// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Prints the time (ms), process id and window class of every window KWin adds, as GGLAT lines; probe.py reads them
// from the user journal to time desktop actions.
workspace.windowAdded.connect(function (w) { print("GGLAT " + Date.now() + " " + w.pid + " " + w.resourceClass); });
