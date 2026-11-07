[Aurora](../README.md) / [Documentation](index.md) / Releasing

# Versions and releases

Aurora follows [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html) and keeps its history in
[CHANGELOG.md](../CHANGELOG.md) in the [Keep a Changelog 2.0.0](https://keepachangelog.com/en/2.0.0/) format. Commits
follow [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/). Each version gets a git tag
`vX.Y.Z` and a GitHub release with a source archive that installs without git.

## What the version number promises

The version covers what a user installs and uses: `./install.sh` and `./uninstall.sh` and their options, the ids and
folders written to the home folder (`org.aurora.*`, `~/.local/share/aurora`, `~/.local/state/aurora`), and the
settings the install changes and the backup restores.

| Change | Version |
| --- | --- |
| A fix that changes nothing a user relies on | patch: 1.0.0 → 1.0.1 |
| A new part, option or behavior, compatible with what is installed | minor: 1.0.1 → 1.1.0 |
| Renamed ids or folders, a removed option, anything that needs a fresh install or that an older backup cannot undo | major: 1.1.0 → 2.0.0 |

A supported Plasma version is not part of the promise: the parts build for whatever Plasma 6 is installed.

## Day to day

1. Commit with a Conventional Commits message: `feat(quick-menu): drag apps within a section`,
   `fix(installer): keep the Dock's pinned apps`, `docs: …`, `test: …`, `chore: …`; `feat!:` or a
   `BREAKING CHANGE:` footer for a major change.
2. In the same change, add what a user would notice to `## [Unreleased]` in CHANGELOG.md, under one of the six types:
   Added, Changed, Deprecated, Removed, Fixed, Security. Write it for the user, not as the commit message; mark a
   breaking entry with **Breaking:**. Internal changes (tests, refactors, docs) need no entry.
3. `git-cliff --unreleased --strip all` ([cliff.toml](../cliff.toml)) drafts entries from the commits since the last
   tag when a lot has piled up; it is a draft to curate, not the entry.
4. `python3 -I scripts/changelog.py check` checks the format; `scripts/check.sh` runs every test.

## Cutting a release

```bash
scripts/release.sh 1.1.0             # checks, dates the changelog, commits, tags v1.1.0, builds the release files
scripts/release.sh 1.1.0 --publish   # the same, then pushes and creates the GitHub release
```

`scripts/release.sh` refuses a dirty tree, a version that is not SemVer or not above the last one, a changelog out of
format, an empty `[Unreleased]`, and a failing check. Then it:

1. moves `[Unreleased]` into `## [1.1.0] - YYYY-MM-DD` and relinks the versions to their comparisons;
2. writes `VERSION`, commits `chore(release): 1.1.0` and tags `v1.1.0` with the release notes;
3. builds `build/release/1.1.0/`: `aurora-1.1.0.tar.gz` (the sources and the themes),
   `release-notes.md` (the version's changelog section) and `SHA256SUMS`;
4. writes the AUR package for that archive in `build/release/1.1.0/aur/`: `PKGBUILD` and `aurora-theme.install` from
   [packaging/aur](../packaging/aur) with `pkgver=1.1.0`, `pkgrel=1` and the archive's SHA-256, and `.SRCINFO`;
5. with `--publish`, pushes the branch and the tag, runs `gh release create v1.1.0` with those files and notes, then
   builds the package into the pacman repository and publishes it (next section).

Without `--publish` it prints the exact publish command, so a release can be reviewed before anything leaves the
machine. Afterwards, copy `build/release/1.1.0/aur/` (`PKGBUILD` and `.SRCINFO`) into `packaging/aur` and commit it
as `chore(packaging): AUR package for 1.1.0`, so the repository's package names the latest release. The release flow is tested in [tests/release/test_release.sh](../tests/release/test_release.sh).

## The pacman repository

Users install `aurora-theme` from Aurora's pacman repository: the files of the GitHub release tagged `repo`, which
[get.sh](../get.sh), README.md's one-command [Installation](../README.md#installation), adds to pacman.conf as
`[aurora]`. get.sh names the key's fingerprint, so a new key is changed there too. The release holds one version:
the package, the `aurora` database, their signatures and `aurora.asc`. Publishing uploads the package first, the
database that lists it next and removes the older build's files last, so the database online never names a missing
package. It refuses a package whose name the release already has: pacman caches packages by name, and makepkg never
builds the same bytes twice, so a rebuild raises `pkgrel`.

```bash
scripts/pacman-repo.sh build/release/1.1.0/aur build/release/1.1.0/repo             # builds and signs, publishes nothing
scripts/pacman-repo.sh build/release/1.1.0/aur build/release/1.1.0/repo --publish   # the same, then replaces the release's files
```

Everything is signed with Aurora's key. Its public half is [packaging/repo/aurora.asc](../packaging/repo/aurora.asc)
(fingerprint `657D8110799A4E65BE4E1AB4F028284112794CC2`), which users trust with `pacman-key --lsign-key`; its secret
half lives only in my GnuPG keyring and an offline copy, never in the repository. A new key means every user trusts
it again, so it is replaced only if the secret half is lost or exposed. `release.sh --publish` runs the script itself;
a change to the package alone (a new dependency) raises `pkgrel` and runs it by hand. The flow is tested in
[tests/release/test_pacman_repo.sh](../tests/release/test_pacman_repo.sh).

The package is built on my PC and installed on everyone's, so it targets the x86-64 baseline, not my processor.
CachyOS's makepkg.conf asks for the building PC's own (`-march=native`, `-C target-cpu=native`), and a package built
that way stops with "Illegal instruction" on an older processor (issue #11). `build()` adds `-march=x86-64
-mtune=generic` and `-C target-cpu=x86-64` after makepkg's flags, and the last one wins in GCC and rustc; makepkg's
hardening flags stay. `check()` then runs [scripts/cpu_baseline.py](../scripts/cpu_baseline.py) on the `aurora`
command and every built part, before they are stripped: an instruction newer than SSE2 is refused unless its
function is built for that instruction set on purpose and named for it (memchr's `find_avx2`, which runs only after
asking the processor). A static library of the build machine counts too: CachyOS's x86-64-v3 libgcc links
`__popcountdi2` with `popcnt`. Both are tested in [tests/release/test_pkgbuild.sh](../tests/release/test_pkgbuild.sh)
and [tests/release/test_cpu_baseline.sh](../tests/release/test_cpu_baseline.sh).

## Publishing to the AUR

The AUR has closed new accounts for now, so `aurora-theme` is not on it yet. When it reopens:

The package is [`aurora-theme`](../packaging/aur/PKGBUILD). It builds the `aurora` command, installs it to
`/usr/bin/aurora` and the theme's files to `/usr/share/aurora`; each user then runs `aurora`, which builds the parts
for their Plasma in `~/.cache/aurora/build` and installs into their home folder. After the GitHub release exists:

```bash
cd build/release/1.1.0/aur
makepkg -f && namcap PKGBUILD *.pkg.tar.zst   # builds from the published archive and lints the package
git clone ssh://aur@aur.archlinux.org/aurora-theme.git ../aur-repo   # once; the AUR account needs an SSH key
cp PKGBUILD aurora-theme.install .SRCINFO ../aur-repo/ && cd ../aur-repo
git commit -am "Update to 1.1.0" && git push
```

A change to the package alone (a new dependency) raises `pkgrel` in `packaging/aur/PKGBUILD`, then the same steps
with `makepkg --printsrcinfo > .SRCINFO`. The first upload needs the repository to be public, since the package
downloads its archive from the GitHub release.

## The GitHub page

- `python3 -I scripts/github_meta.py` shows the description and topics it would set; `--apply` sends them.
- The social preview, what a shared link shows on Discord, X, Reddit and the rest, is
  `assets/readme/social-preview.jpg` (`assets/readme/make.py` draws it). GitHub has no API for it: upload it under
  Settings, Social preview.
- The Apple logo among the menu bar logos is Apple's trademark; Simple Icons' drawing is CC0, the mark is not.

## Installing a release

```bash
tar -xzf aurora-1.1.0.tar.gz && cd aurora-1.1.0 && sha256sum -c ../SHA256SUMS --ignore-missing && ./install.sh
```

`./install.sh --version` prints the version a checkout or archive holds.
