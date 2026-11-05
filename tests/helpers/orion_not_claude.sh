# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Sourced by a scenario: Orion stands where Claude would be, in the Dock and under Developer Tools. Claude's entry is
# hidden and unpinned for the run, Orion's entry is the real one ($REALHOME) so its icon shows, and the app catalog is
# rebuilt. orion_not_claude_end puts Claude back; call it after plasmashell has quit.
# shellcheck shell=bash
ORION_APPS="$HOME/.local/share/applications"
ORION_APPLETS="$HOME/.config/plasma-org.kde.plasma.desktop-appletsrc"
ln -sfn "$REALHOME/.local/share/applications/orion-app.desktop" "$ORION_APPS/orion-app.desktop"
cp -a "$ORION_APPS/com.anthropic.Claude.desktop" "$SBX/run/claude.desktop.bak"
printf '[Desktop Entry]\nType=Application\nName=Claude\nNoDisplay=true\n' > "$ORION_APPS/com.anthropic.Claude.desktop"
ORION_PINS=$(grep -m1 '^launchers=' "$ORION_APPLETS")
sed -i 's|applications:com.anthropic.Claude.desktop,||' "$ORION_APPLETS"
# Plasma's menu is plasma-applications.menu; the session sets this prefix, the sandbox's clean environment does not.
export XDG_MENU_PREFIX=plasma-
kbuildsycoca6 --noincremental > "$SBX/logs/kbuildsycoca.log" 2>&1
# orion_not_claude_end: restores Claude's entry and the pins the Dock had before the run.
orion_not_claude_end() {
    cp -a "$SBX/run/claude.desktop.bak" "$ORION_APPS/com.anthropic.Claude.desktop"
    sed -i "s|^launchers=.*|$ORION_PINS|" "$ORION_APPLETS"
}
