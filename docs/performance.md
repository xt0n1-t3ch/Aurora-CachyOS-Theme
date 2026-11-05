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
