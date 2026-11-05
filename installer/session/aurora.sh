# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Aurora, sourced by Plasma before KWin and plasmashell start (~/.config/plasma-workspace/env).
# KWin and Plasma load the theme's window buttons, system tray and notifications from the user's plugin folder.
# The variables and functions below are unset at the end, so they do not outlive this file.
# shellcheck shell=sh
aurora_state="$HOME/.local/share/aurora"
aurora_plugins="$HOME/.local/lib/qt6/plugins"
export QT_PLUGIN_PATH="$aurora_plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
# The Control Center, Quick Menu and the banners frost their glass through org.aurora.glass, a QML module in the
# user's Qt 6 folder.
export QML_IMPORT_PATH="$HOME/.local/lib/qt6/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
# Open and Save panels come from File Explorer through xdg-desktop-portal (see ~/.config/environment.d).
export PLASMA_INTEGRATION_USE_PORTAL=1 GTK_USE_PORTAL=1

# These plugins are built for the installed Plasma, KDecoration and Qt (installer/build.sh writes what each was built
# for). After an update that changes those, each one steps aside until ~/.local/share/aurora/rebuild.sh builds
# it again: KWin falls back to Breeze, Plasma to its stock tray and notifications, and nothing loads a plugin built
# for other libraries.
# Prints the installed version of package $1 without the package release.
aurora_version() {
    pacman -Q "$1" 2>/dev/null | awk '{print $2}' | sed 's/-[^-]*$//'
}
# Pauses a plugin when the versions it was built for no longer match: aurora_keep_if_built_for PART VERSIONS PLUGIN.
# PART names the stamp that installer/install.py saved, VERSIONS is what is installed now and PLUGIN is the file that
# is renamed to PLUGIN.paused.
aurora_keep_if_built_for() {
    if [ "$2" != "$(cat "$aurora_state/$1-built-for" 2>/dev/null)" ] && [ -e "$3" ]; then
        mv -f "$3" "$3.paused"
    fi
}
aurora_plasma="$(aurora_version plasma-workspace) $(aurora_version qt6-base)"
aurora_keep_if_built_for system-tray "$aurora_plasma" "$aurora_plugins/plasma/applets/org.kde.plasma.systemtray.so"
aurora_keep_if_built_for notifications "$aurora_plasma" "$aurora_plugins/plasma/applets/org.kde.plasma.notifications.so"
aurora_keep_if_built_for window-buttons "$(aurora_version kdecoration | cut -d. -f1,2)" \
    "$aurora_plugins/org.kde.kdecoration3/org.aurora.decoration.so"
unset -f aurora_version aurora_keep_if_built_for
unset aurora_state aurora_plugins aurora_plasma
