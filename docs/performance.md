[Aurora](../README.md) / [Documentation](index.md) / Performance

# Measured tuning

Every value here was measured on my own machine before it was kept: a Ryzen 7 5800X with 32 GB of RAM and an RTX 4070
Ti SUPER, on linux-cachyos 7.3.1 (2026-11-04). Your machine may differ, which is why the tuning is optional. `aurora performance on` applies them from [`performance/system/`](../performance/system) and `aurora performance off` puts CachyOS's settings back (`installer/root-steps.sh` `tune` and `untune`, one sudo call each; `./install.sh --performance` does the same as part of an install); the measuring kit is in
[`performance/measure/`](../performance/measure). Re-measure before changing any of them.

## CPU scheduler: scx_lavd (autopilot)

`/etc/scx_loader.toml` (`default_sched = "scx_lavd"`, `default_mode = "Auto"`), `scx_loader.service` enabled,
scx-scheds 1.1.4. On EEVDF every app has its own systemd cgroup at equal weight, so ananicy-cpp's nice values on
compilers never reach other apps; lavd prioritises latency-critical tasks.

Alternating rounds under 32 busy background threads (`performance/measure/probe.py`, `under-load.sh`, `sched/`), with the CPU other
sessions used logged per round:

| | EEVDF | scx_lavd | bpfland | cosmos |
| --- | --- | --- | --- | --- |
| Emoji picker (Win+.) under load | 1099 ms | **666 ms** | 851 ms | 780 ms |
| Konsole under load | 819 ms | **674 ms** | | |
| 7-Zip, scaled to an idle machine | 96-97k MIPS | **101-108k MIPS** | | |
| SCHED_RR 1 ms timer, max | 160 µs | 373 µs (far under a 4.2 ms frame) | | |

lavd `--performance`, flash and p2dq did worse; no scheduler was ejected. BORE ships only as `linux-cachyos-bore`
(a kernel switch, and its 7.3.1 build was broken on 2026-10-31).

## Memory

- **KWin and plasmashell never swap** (`MemorySwapMax=0`, `installer/session/noswap.conf`): with zram and
  CachyOS's swappiness of 150, 795 MB of KWin and Xwayland had been swapped out on 2026-11-04, and switching apps
  stuttered while those pages came back.
- **zram stays** (CachyOS: size = RAM, `vm.swappiness=150` from `/usr/lib/udev/rules.d/30-zram.rules`,
  `page-cluster=0`, zswap off, systemd-oomd active). It is compressed swap inside RAM: 10.3 GB of idle pages held in
  3.3 GB (3.1:1), no OOM kill in 39 h. The kernel docs recommend swappiness above 100 for in-memory swap.
- **`vm.watermark_scale_factor = 125`** (`/etc/sysctl.d/99-aurora-memory.conf`). Before: 244,128 direct-reclaim
  stalls (`allocstall`) and 120,680 early kswapd sleeps (`kswapd_low_wmark_hit_quickly`) in 39 h, the two signs the
  kernel docs give for raising it. In the first 15 minutes after, under the same swap traffic: 0 stalls (about 1,560
  on average before) and 421 early sleeps (about 770). Cost: about 400 MB kept free.
- **zram lz4** (`/etc/systemd/zram-generator.conf.d/90-aurora-zram.conf`, from the next boot). On this CPU with the
  same 64 KiB blocks: decompression 3.7 GB/s (zstd 1.3), compression 717 MB/s (zstd 347); ratio 1.9 against 2.5.
  Cost: about 1 GB more RAM held by zram at 10 GB swapped; under extreme pressure zram's extra room ends sooner.
- KWin and plasmashell have `MemorySwapMax=0` (switching apps used to fault swapped KWin pages back and stutter).
- THP defrag `defer` (`/etc/tmpfiles.d/thp.conf`) after 279k direct-compaction stalls.
- MGLRU `min_ttl_ms` stays at 100: oomd is present.

## Disk I/O scheduler: kyber stays

CachyOS's kyber against ADIOS 3.2.0 on both NVMe disks (2026-11-07, `performance/measure/iosched/ab.py`, results in
`result-2026-11-07.json`): 4 KiB direct random reads timed while two writers stream direct 1 MiB writes, in
alternating 15 s windows. Flat out there was no clean winner; at a realistic load kyber did as well or better.

- nvme0 (ADATA LEGEND 700, `/`): read median under load kyber 954, 3119, 3901 µs against ADIOS 1372, 1402, 1413 µs;
  p99 2.5-10.3 ms against 3.1-7.9 ms. Kyber won the first pair, while the drive was fresh. ADIOS won the later pairs,
  after the drive's write speed fell from 1.24 GB/s to about 0.8 GB/s, and its writers kept more speed (0.9-1.3 GB/s
  against 0.8-1.2).
- nvme1 (Kingston OM8PCP3, `/mnt/D`): the drive's own state ruled. Its writes fell from 618 MB/s to 22 MB/s within
  minutes, and reads stalled up to 0.73 s whichever scheduler ran. ADIOS had the two best loaded windows (p99 0.7 and
  3.7 ms against 25-47 ms for kyber) and also the single worst one (p99 280 ms).
- Unloaded, both read in 77-98 µs at the median. The disk's slowdown under sustained writes costs far more than the
  scheduler choice: builds and downloads on `/mnt/D` stall reads for tens of ms.
- CachyOS keeps ADIOS optional because it is still in development and can lock up in rare cases
  ([wiki](https://wiki.cachyos.org/configuration/general_system_tweaks/)).
- Rerun with a realistic load (`result-2026-11-07-realistic.json`): two buffered writers at 75 MB/s each (150 MB/s,
  a fast download or a build), 20 s of rest before each window, 5 alternating rounds. nvme0: read median 125 µs for
  both; p99 median kyber 455 µs (400-906) against ADIOS 882 µs (409-1407), reads per second 6936 against 6479, kyber
  better in 3 of 5 pairs. nvme1: p99 median kyber 1.5 ms against ADIOS 1.8 ms, reads per second 6055 against 2090,
  kyber better in 3 of 5 pairs; both had windows where the drive stalled (kyber up to 1.1 s, ADIOS up to 0.35 s). At a
  realistic load kyber is as good or better, which settles it.
- The Kingston stalls reads for tens of ms, and once for 1.1 s, even at 150 MB/s of writes and even with the disk
  quiet (p99.9 23 ms). The ADATA never went past 51 ms. Heavy writes belong on the ADATA (`/`, `/home`).

## File Explorer's search

Measured 2026-11-11 on this PC (Ryzen 7 5800X, NVMe btrfs), the helper run as the user with `root = /usr`, 490,544
names, 4 reading threads at idle priority:

| What | Measured |
| --- | --- |
| First index of /usr | 1,125 ms; 3.96 s of CPU over the reading threads |
| Memory | 77.6 MB after the first index; 48.8 MB after a start from the stored index |
| Index file | 21.9 MB |
| Start from the stored index, catch up by folder times | 142 ms |
| A query, a new `aurora-search-query` process each time included (best of 5) | `libQt6Core` 1.9 ms, `python3` 1.9 ms, `kio` 2.6 ms, `.so` 4.6 ms, `ext:h` 11.5 ms, `regex:^lib.*\.so\.6$` 15.5 ms |
| Wakeups in 3 s with a client connected and nothing to do | 0 |

A word is found with one SIMD pass over every name; `ext:`, `regex:` and wildcards test every name, so they grow
with the index (about 5 ms per 100,000 names here). With no client and no work the helper leaves; at rest nothing
runs. The root-only parts (fanotify, the btrfs catch-up) are timed in the virtual machine test: the catch-up after a
restart answered in under 3 s there, the bound the test sets (`tests/vm_server.rs`).

## Mission Center's glass

Measured in Mission Center's sandbox (`components/mission-center/tools/sandbox`, a nested KWin at 60 Hz, 2026-11-07):
the window on the CPU page over Aurora's wallpaper, 15 s to settle, then the CPU used over 30 s, as a share of one
core. With smooth and sliding graphs on (they redraw every frame), the UI of package 1.2.0.r52-5 and -6 used 20.7 and
19.5 %; -7, whose glass asked GTK for KWin's blur with CSS `backdrop-filter`, used 66.3 % (6.6 % against 4.4 % with
the graphs still). On the desktop's 240 Hz screen it used 52 %. magpie stayed at 4.5 % in every build.

- GTK 4.24 builds `backdrop-filter` into every frame as copy, paste and blur nodes, and to find the region it sends
  KWin it walks the frame (gsk/gskblurutils.c): with a copy around the whole window, it rasterized every clip and graph
  line on the CPU each frame. The workspace's dev build used 66.6 % as it was, 29.3 % without `backdrop-filter` and
  20.1 % with no blur and no tint at all.
- The same nodes around nothing but the paste, from the window's own snapshot, still cost 44.8 %, cached or not: any
  copy node costs every frame.
- Mission Center now keeps GDK off ext-background-effect-v1 (`GDK_WAYLAND_DISABLE`) and asks KWin for the blur itself,
  once per size or state (`src/glass.rs`): 21.4 %, as with no blur. The tint lost its path too: drawn as the sidebar
  card's shadow with no blur, it costs nothing measurable. Package -8, built so: 20.0 % with the graphs animated and
  4.8 % with them still, as before the glass; on the desktop, with the same settings and page, 22 % where -7 used
  52 %.

## Power

- Power profile stays **Performance** (power-profiles-daemon 0.30, amd_pstate EPP; it changes only the CPU's
  EPP/governor). `performance/measure/power/ab.sh` at the same light load: Balanced 44-47 W against 49-56 W package, but the first
  burst after idle 6.1 ms against 5.5 ms (11 % slower). About 7 W, roughly $0.60 a month: not worth it.
- The GPU idles at P5, about 14 W. The CPU package averaged 117 W only while another session's test run used 9 cores.
- Edge tabs playing video hold a "Video Wake Lock" that keeps the screens on.
- Panama's tariff (ASEP, Sept-Dec 2026): above 300 kWh a month the FET subsidy is lost and each extra kWh costs
  $0.23-0.25; a $105 bill is about 500 kWh.

## Display and Edge

- Edge runs on native Wayland (`--ozone-platform=wayland --enable-blink-features=CanvasHDR` in
  `~/.config/microsoft-edge-stable-flags.conf`) for HDR/WCG and exact 240 Hz; computer-use cannot see it there.
- The Samsung DP-1 has VRR "Automatic", not "Always" ("Always" drops the cursor to the content's rate).
