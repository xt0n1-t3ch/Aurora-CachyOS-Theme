#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Prints layout.js with its placeholders filled in for a home directory.
# The three gaps (GAP_TRAY_SEARCH, GAP_SEARCH_CC and GAP_CC_CLOCK, which the environment can override) top each pair
# of right-hand applets up to the 46 px from one tray icon to the next. Measured in the sandbox with bar_glyphs.py,
# the tray arrow, Quick Menu, Control Center and the clock then sit 46 px apart, as the tray's own icons do.
#
#   render_layout.sh HOME
here=$(dirname "$(readlink -f "$0")")
sed -e "s|@HOME@|$1|g" -e "s|@GAP_TRAY_SEARCH@|${GAP_TRAY_SEARCH:-2}|" -e "s|@GAP_SEARCH_CC@|${GAP_SEARCH_CC:-11}|" \
    -e "s|@GAP_CC_CLOCK@|${GAP_CC_CLOCK:-1}|" "$here/layout.js"
