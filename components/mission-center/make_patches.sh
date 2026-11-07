#!/bin/sh
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# Writes the four patches the PKGBUILD applies, one per upstream repository, from the clones Mission Center is
# developed in (mc, gng, app-detection and graph-widget next to this script, kept out of git). Submodule pointers (the
# development links between the clones) and local measuring tools are left out.
#
#   components/mission-center/make_patches.sh     then update the PKGBUILD's sha256sums (updpkgsums)
set -eu
ws=$(dirname "$(readlink -f "$0")")

# patch_of REPO NAME EXCLUDED_PATH...: writes NAME.patch, the clone's tracked changes plus its untracked files, minus
# the excluded paths, and prints how many files it covers.
patch_of() {
  repo=$1 name=$2; shift 2
  excludes=""
  for path in "$@"; do excludes="$excludes :(exclude)$path"; done
  cd "$ws/$repo"
  # shellcheck disable=SC2086
  git diff --no-ext-diff -- . $excludes > "$ws/$name.patch"
  # shellcheck disable=SC2086
  for file in $(git ls-files --others --exclude-standard -- . $excludes); do
    git diff --no-ext-diff --no-index -- /dev/null "$file" >> "$ws/$name.patch" || true
  done
  echo "$name.patch: $(grep -c '^+++ ' "$ws/$name.patch") files"
}

patch_of mc mission-center-improved subprojects/magpie subprojects/graph-widget
patch_of gng magpie-improved platform-linux/crates/app-rummage examples/subsystem_cost.rs
patch_of app-detection app-rummage-improved
patch_of graph-widget graph-widget-improved
