[Aurora](../README.md) / [Documentation](index.md) / Search

# File Explorer's whole-disk search

File Explorer finds any name on the PC as you type, from an index kept by `aurora-search`
([`components/file-explorer/search`](../components/file-explorer/search), Rust). This page says how it stays current
without running at rest, who sees what, and how it is tested.

## When it runs

- systemd holds the socket `/run/aurora/search.sock` (`aurora-search.socket`) and starts the helper on the first
  connection. Nothing runs before that.
- File Explorer connects when one of its windows becomes active or a search starts, and lets go after three minutes
  without either (`fe::SearchService`). A window left open all day does not keep the helper running.
- With no client left, the helper waits its idle time (`idle_minutes`, 5 by default), writes the index if it changed,
  and exits. Measured: 0 wakeups while it waits ([performance.md](performance.md#file-explorers-search)).

## How it stays current

The helper keeps every name of every local filesystem (from `/proc/self/mountinfo`; network shares, snapshots and
`/proc`, `/sys`, `/dev`, `/run`, `/tmp`, `/var/tmp` stay out) in one index, written to
`/var/lib/aurora/search/index` when it leaves (atomically, with a CRC; a damaged file is ignored and the disks are read
again).

- **While it runs**, fanotify reports every create, delete, move and attribute change on each filesystem, naming the
  folder by a file handle; the helper reads those folders again in batches 150 ms apart. It never listens to writes
  inside files (`FAN_MODIFY` comes with every `write()`). A full event queue starts a catch-up.
- **When it starts**, it catches up on what changed while it was off. On btrfs it commits the running transaction (a
  change reaches btrfs's tree only then: until the commit the new inode stays among the delayed inodes, where a tree
  search does not look) and asks for the folders whose inode a newer transaction changed (`BTRFS_IOC_TREE_SEARCH_V2`
  with `min_transid`), the part the change journal plays for Everything on NTFS. Elsewhere it compares each folder's
  modification time with the stored one, as `updatedb` does. FAT and exFAT, whose folder times are unreliable, are
  read again whole.
- A folder whose name moved keeps its entry and everything under it: a batch removes first and adds after, so a name
  that left one folder for another is moved, not read again.
- Each match on screen is checked with `lstat` by the KIO worker, so a file deleted a moment ago is never listed.

## Who sees what

The socket accepts every local user. For each query the helper switches its own thread's filesystem identity to the
asker's (`setfsuid`, `setfsgid` and `setgroups` as raw syscalls, which change one thread; glibc's wrappers change
all) and asks the kernel with `faccessat2(..., AT_EACCESS)` whether the asker could list the match's folder and enter
every folder above it, so modes, groups and ACLs all count. A match the asker could not reach is neither shown nor
counted. The service runs as root with only `CAP_SYS_ADMIN` (fanotify on whole filesystems, btrfs's tree search),
`CAP_DAC_READ_SEARCH` (reading folders, opening handles), `CAP_SETUID` and `CAP_SETGID`, read-only home, no network.

## The query syntax

Words must all match the name, without regard to case; `|` gives alternatives, `!` excludes; a word with `/` matches
the path; `*` and `?` match the whole name; `ext:jpg;png`, `regex:`, `folder:`, `file:`, `size:>10mb`,
`dm:today`, `dm:<7d`. Hidden names are left out unless the query holds `hidden:`, which File Explorer adds while the
view shows hidden files. Matches are ranked: the name equal to the first word, then starting with it, then a word of
the name starting with it, then anywhere; shallower and shorter first.

## Settings

`/etc/aurora/search.conf`: `idle_minutes`, and `exclude = /path` (one per line). `aurora-search-query WORDS` asks from
a terminal (`--scope DIR`, `--limit N`).

## Tests

`cargo test` covers the index (random histories against a model of the disk), the queries, the scan, the store, the
catch-up by folder times, mountinfo, fanotify's event layout and the settings. `tests/vm.sh` runs the root tests in a
virtme-ng machine on this PC's own kernel, so they get root, fanotify and a fresh btrfs without sudo: the btrfs
catch-up, and the helper from start to exit (index, live create, move and delete, a `0700` folder hidden from
`nobody`, scopes, idle exit, catch-up after a restart). The sandbox's `explorer-search.sh` drives File Explorer
against a helper run as the user. See [../tests/index.md](../tests/index.md).
