[Aurora](../README.md) / Documentation

# Aurora documentation

How the theme is built, installed, kept working across Plasma updates, undone and tested. Start with the task you have,
then follow the part it touches.

**State on 2026-11-04:** every part below is live on CachyOS with Plasma 6.8.2, KWin 6.8.2, Qt 6.12.0, kernel
linux-cachyos 7.3.1, an NVIDIA RTX 4070 Ti SUPER and a Ryzen 7 5800X.

[Start here](#start-here) · [How it works](#how-it-works) · [Maintain it](#maintain-it) · [Rules](#rules)

## Start here

| I want to… | Read |
| --- | --- |
| Install the theme | [README: Install](../README.md#installation) · [install.md](install.md) |
| Bring the theme's parts back after a system update | [update.md](update.md) |
| Go back to the desktop I had, all of it or one part | [restore.md](restore.md) |
| Know what the optional performance tuning does, and why | [performance.md](performance.md) |
| Try a change without touching my desktop | [sandbox.md](sandbox.md) |
| Run the tests | [../tests/index.md](../tests/index.md) |

## How it works

| Page | What it answers |
| --- | --- |
| [architecture.md](architecture.md) | How each part hooks into Plasma, KWin, systemd and D-Bus; every file the install adds, inside and outside the home folder |
| [components.md](components.md) | Each part: what it does, its source, how it is built, tested, installed and kept working across updates |
| [search.md](search.md) | File Explorer's whole-disk search: the helper, how it stays current without running at rest, who sees what |
| [build.md](build.md) | What `installer/build.sh` builds and how, and how to build one part by hand |

## Maintain it

| Page | What it answers |
| --- | --- |
| [update.md](update.md) | What steps aside after an update, how `rebuild.sh` brings it back, how to port a patch to a new Plasma |
| [restore.md](restore.md) | What `./uninstall.sh` undoes, and how to undo one part at a time |
| [releasing.md](releasing.md) | Versions (SemVer), the changelog (Keep a Changelog), commit messages and `scripts/release.sh` |
| [sandbox.md](sandbox.md) | The nested Plasma session for tests and captures: running it, driving it, its gotchas |
| [../tests/index.md](../tests/index.md) | Every test suite and sandbox scenario, the command that runs it and its last result |

## Rules

| Page | What it answers |
| --- | --- |
| [decisions.md](decisions.md) | The design rules and the decisions behind them |
| [../NOTICE.md](../NOTICE.md) | Aurora's license, the attribution terms, and which license covers each part |
| [../llms.txt](../llms.txt) | The map of these pages for language models and coding agents; `scripts/llms.py` writes it and `llms-full.txt` |
