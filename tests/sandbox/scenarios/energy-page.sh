#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Info Center > Energy with the theme's page: opened straight on the module, captured after 12 s of samples.
source "$SBX/prelude.sh"
QT_FORCE_STDERR_LOGGING=1 kinfocenter kcm_energyinfo > "$SBX/logs/kinfocenter.log" 2>&1 &
waitfor 14
shot energy-page
