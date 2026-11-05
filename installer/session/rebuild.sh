#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Builds the theme's parts again for the Plasma, KWin and Qt now installed, then installs them. Run it after a
# system update paused some of them (the login script ~/.config/plasma-workspace/env/aurora.sh does that when
# the versions change), then log out and back in. The checkout it builds from is ~/.local/share/aurora/source.
exec "$HOME/.local/share/aurora/source/install.sh" --rebuild "$@"
