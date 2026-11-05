#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# "Show in folder": a FileManager1.ShowItems call opens File Explorer on the file's folder with the file selected.
source "$SBX/prelude.sh"
TAG="${SCEN_TAG:-now}"
QT_FORCE_STDERR_LOGGING=1 plasmashell > "$SBX/logs/plasmashell.log" 2>&1 &
waitfor 10
"$SB/build/file-explorer/file-explorer" --service > "$SBX/logs/fm1-$TAG.log" 2>&1 &
waitfor 2
gdbus call --session --dest org.freedesktop.FileManager1 --object-path /org/freedesktop/FileManager1 \
    --method org.freedesktop.FileManager1.ShowItems "['file://$HOME/Downloads/reveal-me.txt']" "" >> "$SBX/logs/fm1-$TAG.log" 2>&1
waitfor 4
shot "fm1-$TAG"
gdbus introspect --session --dest org.freedesktop.FileManager1 --object-path /org/freedesktop/FileManager1 >> "$SBX/logs/fm1-$TAG.log" 2>&1
