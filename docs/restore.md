[Aurora](../README.md) / [Documentation](index.md) / Restore

# Going back

## Everything: `aurora uninstall`

`aurora uninstall` (or `./uninstall.sh` in a source copy) runs `restore.sh` in the backup that `~/.local/state/aurora/latest-backup` names, the one the first install
made. The restore stops plasmashell, deletes every path in `created.txt`, puts back every saved setting from `home/`,
unsets the session environment, gives Win+E back to Dolphin and the Windows key back to Plasma's launcher (with the
`windows_key.py` saved beside it), reloads D-Bus and the portal, restarts KRunner as Plasma ships it, removes the two
plugin links and, when present, the performance tuning (one password prompt each), puts the notification corner and
the GTK theme back, and starts plasmashell. Log out and back in afterwards. The backup stays where it is.
`aurora backups` lists the backups with the time each was taken and how many settings it saved, and
`aurora backups restore NAME` runs the `restore.sh` of another one.

## One part at a time

| Part | Undo |
| --- | --- |
| Quick Menu | `rm ~/.config/systemd/user/plasma-krunner.service.d/aurora.conf && systemctl --user daemon-reload && systemctl --user restart plasma-krunner.service` |
| Windows key | `python3 -I components/windows-key/windows_key.py stock` |
| Notification banners | `mv ~/.local/lib/qt6/plugins/plasma/applets/org.kde.plasma.notifications.so{,.paused}`, then restart plasmashell |
| Notification corner | `kwriteconfig6 --file plasmanotifyrc --group Notifications --key PopupPosition --delete` |
| System tray | `mv ~/.local/lib/qt6/plugins/plasma/applets/org.kde.plasma.systemtray.so{,.paused}`, then restart plasmashell |
| Window buttons | System Settings > Colors & Themes > Window Decorations > Breeze |
| CPU scheduler | `sudo systemctl disable --now scx_loader.service` |
| Memory tuning | `sudo rm /etc/sysctl.d/99-aurora-memory.conf /etc/systemd/zram-generator.conf.d/90-aurora-zram.conf && sudo sysctl -w vm.watermark_scale_factor=10` |

A new part means a new entry in `ADDED` or `SAVED` and, when it needs more than deleting files, a line in the
`RESTORE` template of `installer/install.py`.
