# Aurora: copyright, license and attribution

**Aurora by xt0n1** ([github.com/xt0n1-t3ch](https://github.com/xt0n1-t3ch)).
Copyright (C) 2026 xt0n1.

Aurora's own code is free software: you can redistribute it and modify it under the terms of the GNU General Public
License version 3 ([LICENSE](LICENSE)), together with the additional terms below. Every file of Aurora's own code says
so in its header (`SPDX-License-Identifier: GPL-3.0-only` and a line pointing here). It comes with no warranty, to the
extent permitted by law; see sections 15 and 16 of the license.

## Additional terms (GNU GPL version 3, section 7)

These terms supplement the GNU GPL version 3 for the files whose header points to this notice, and for every work
based on them.

1. **Author attribution, section 7(b).** Keep the author attribution
   "Aurora by xt0n1 (https://github.com/xt0n1-t3ch)" in the source files that carry it, and in the Appropriate Legal
   Notices of every interactive interface of the work: today the `aurora` command's banner ("Developed by xt0n1")
   and its `license` command. As section 5(d) allows, a modified version may change where and how those notices appear, as long as
   they still name the author.
2. **Modified versions, section 7(c).** Do not present a modified version as the original. Mark it in a reasonable
   way as different from it, for example "based on Aurora by xt0n1", so nobody credits the author with changes they
   did not make, or blames them for those changes.
3. **The author's name, section 7(d).** The name "xt0n1" may not be used to promote a modified version, or to
   suggest the author endorses it, without the author's written permission.

## What else is in this repository

The parts that come from other projects keep their own license: each file's header, or the license file in its
folder, is the final word.

| Files | License |
| --- | --- |
| Aurora's own code: File Explorer, the window buttons, glass, the pointing hand, the energy page's C++, the clock, the Dock divider, the installer, the `aurora` command, the scripts and tests | GPL-3.0-only with the additional terms above |
| Plasma's parts rebuilt with a patch (`components/system-tray`, `components/notifications`, `components/quick-menu`: `upstream/`, `vendor/`, the `.patch` files), and other files whose header says GPL-2.0-or-later | GPL-2.0-or-later, by KDE's authors; Aurora's changes to them are under the same license |
| Mission Center's patches (`components/mission-center/*.patch`): changes to Mission Center, magpie and graph-widget, and to app-rummage | GPL-3.0-or-later like Mission Center, magpie and graph-widget, by their authors; MIT like app-rummage for its patch |
| Dawn Bird, Aurora's wallpaper (`themes/wallpapers/Aurora`), and the README's pictures (`assets/readme`, `assets/screenshots`) | CC BY-SA 4.0, by xt0n1 |

The names Apple, macOS, Tahoe, CachyOS, Arch Linux, Kali Linux and KDE are their owners' trademarks. Aurora is
inspired by macOS Tahoe and is not affiliated with Apple.
