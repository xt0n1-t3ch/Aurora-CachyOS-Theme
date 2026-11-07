[Aurora](../README.md) / [Documentation](index.md) / Updates

# Updates

## New versions of Aurora

Each release is built once, on the maintainer's machine, by `scripts/release.sh X.Y.Z --publish`: the package with
every part built goes into the signed pacman repository (the GitHub release named `repo`), so no user compiles
anything. pacman brings it with the rest of the system (`sudo pacman -Syu`), or `aurora update` brings it the safe
way described below.

Aurora also says when one is out. `aurora-updates.timer`, a user timer the install enables in
`~/.config/systemd/user`, runs `aurora check-updates --notify` once a day (and at the next login when the PC was off).
That looks in pacman's repositories with `checkupdates` (its own copy of the databases in `~/.cache/aurora/checkup-db`,
no root) and, when a newer aurora-theme is there, shows a notification. Only a new aurora-theme does: updates of
any other package never show it. Its buttons:

| Button | What happens |
| --- | --- |
| Update now | Konsole opens with `aurora update` |
| Skip this version | no notice offers that version again; the next one is offered |
| (closed) | it asks again at the next check |

`aurora settings` chooses how often it looks: every day (the default), every week or never
(`aurora settings --updates weekly` from a script). The choice is in `~/.config/aurora/aurora.conf`; what the last
look found is in `~/.local/state/aurora/updates`, and `aurora` names a found version above its menu.
`aurora check-updates` looks right away.

## How `aurora update` installs it

`aurora update` changes nothing when no newer aurora-theme is out. When one is, it reads what that package was built
for before installing anything: pacman packages begin with a `.BUILDINFO` file that lists the packages of the machine
that built them, so it downloads only the first mebibyte of the package (the first 128 KiB held all of 1.1.1's). It
compares those versions with this PC the way the login script compares the parts' stamps: KDE Plasma, KWin and Qt
whole, KDecoration by its minor version.

| The new package is built for | `aurora update` runs | What changes |
| --- | --- | --- |
| what a system update brings | `sudo pacman -Syu --needed aurora-theme` | Aurora and the rest of the system together, as Arch expects |
| this PC as it is, while a system update would bring another Plasma, KWin, Qt or KDecoration | `sudo pacman -U` with the package's address | only Aurora; the system update waits for Aurora's next build |
| neither | nothing | it names the versions that differ and how to update the system anyway |

pacman checks the package's signature in every case: `pacman -U` with an address takes pacman.conf's `SigLevel`
unless `RemoteFileSigLevel` says otherwise. An aurora-theme built from the AUR is in no repository, so its AUR helper
updates it with the whole system, as before.

# After a system update

CachyOS updates with `sudo pacman -Syu` (or `paru`). The theme's compiled parts are built for the installed Plasma,
KWin, KDecoration and Qt. When those change, each part steps aside by itself and the stock component takes over:

| Changed package | What happens until the rebuild |
| --- | --- |
| plasma-workspace or qt6-base | at login the tray and the notifications applet are renamed `*.paused` and Plasma loads its own; Quick Menu's launcher starts `/usr/bin/krunner` |
| kdecoration | the window buttons are paused at login; KWin uses Breeze |
| kwin | KWin refuses the pointer plugin (no hand over window buttons) |

Nothing breaks; the desktop only loses the theme's look of those parts. To bring them back:

```bash
~/.local/share/aurora/rebuild.sh      # runs ./install.sh --rebuild in the checkout; then log out and in
```

It checks the packages, builds every part for the new versions ([build.md](build.md)), installs the built parts
beside what they were built for, refreshes the root links and loads the pointer plugin. Settings and panels stay as
they are.

## When a patch no longer applies

`build.sh` builds a Plasma part for a new Plasma version from that version's own sources with the part's patch. If the
new Plasma changed the same lines, the patch fails, the part is skipped and Plasma keeps its own. To port it:

1. Fetch the new upstream files into `components/<part>/upstream/` (the same sparse clone as `build.sh`, folders in its
   `sparse_paths`).
2. Port `vendor/` onto them: keep the theme's changes (they are marked `Aurora:` in comments) and take every
   upstream change. Update the version in the component's `project(... VERSION ...)`.
3. Regenerate the patch as the difference from `upstream/` to `vendor/` (`diff -ruN` of two copies named `a` and `b`,
   leaving out the binary `*.qsb`), then check that `patch -p1` on a fresh copy of `upstream/`, plus the `.qsb`,
   reproduces `vendor/` exactly.
4. Run the part's tests and sandbox scenario, then `installer/build.sh <part>` and `install.py components`.

## The themes

Aurora's themes live in `themes/` and change like any other file: edit, reinstall what the install copies
(`install.py files`), test in the sandbox, commit. The base icons are a built archive
(`themes/icons/base/Aurora-base.tar.xz`, made reproducibly with `tar --sort=name --mtime`).

## A new Plasma for the package

The aurora-theme package carries its parts built for one Plasma, KWin and Qt. When Arch or CachyOS ships a new one,
raise `pkgrel` in `packaging/aur/PKGBUILD`, build the package again on an up-to-date machine and publish it to the
pacman repository ([releasing.md](releasing.md)); until then, each user's login script sets aside the parts built for
the older version and Plasma uses its own, and `aurora rebuild` builds them for a user who does not want to wait.
