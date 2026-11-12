# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Sourced by scenarios: starts the minimal Plasma services inside the sandbox and defines shot and waitfor.
# shellcheck shell=bash
export QT_QPA_PLATFORM=wayland
# Services that D-Bus starts on demand see the variables exported above.
dbus-update-activation-environment --all
# The activity manager and kded are the services a Plasma session brings up before the shell.
/usr/lib/kactivitymanagerd > "$SBX/logs/kamd.log" 2>&1 &
kded6 > "$SBX/logs/kded.log" 2>&1 &
# File Explorer starts every scenario from its defaults: the settings a scenario switches on (a view mode, the
# Inspector, archives as folders, search everywhere) would otherwise carry over into the next one. A scenario that
# needs one sets it after sourcing this file.
for setting in View:Mode Inspector:Visible General:ArchivesAsFolders Search:Everywhere; do
    kwriteconfig6 --file file-explorerrc --group "${setting%%:*}" --key "${setting#*:}" --delete
done
# shot NAME: captures the whole screen to shots/NAME.png with Spectacle; its output goes to logs/spectacle-NAME.log.
shot() { spectacle -b -n -f -o "$SBX/shots/$1.png" > "$SBX/logs/spectacle-$1.log" 2>&1; }
# waitfor SECONDS: sleeps one second at a time, SECONDS times; seq drops a fraction, so 1.5 waits 1 s.
waitfor() { for _ in $(seq 1 "$1"); do sleep 1; done; }
