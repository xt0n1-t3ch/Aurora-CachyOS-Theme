# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Sourced by a scenario: showcase_wallpaper puts the wallpaper of every Aurora picture on the sandbox's desktop. It is
# Dawn Bird, Aurora's own wallpaper (themes/wallpapers/Aurora), at its largest size, which the sandbox's 2x captures
# need. Call it once plasmashell runs. Prints why and returns 1 when the image is missing.
# shellcheck shell=bash
showcase_wallpaper() {
    local image="$SB/themes/wallpapers/Aurora/contents/images/5120x2880.jpg"
    if [ ! -f "$image" ]; then
        echo "showcase_wallpaper: $image is missing" >&2
        return 1
    fi
    plasma-apply-wallpaperimage "$image" > /dev/null 2>&1
}
