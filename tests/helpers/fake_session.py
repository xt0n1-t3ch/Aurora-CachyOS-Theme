# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""fake_session.py LOG: stands in for Plasma's logout prompt, its shutdown service and the screen locker on the
sandbox's session bus, and writes each call it gets to LOG ("org.kde.LogoutPrompt.promptReboot"), doing nothing else.

A scenario starts it before it clicks Log Out, Restart, Shut Down or Lock, so those calls reach this script and never
the real services: Plasma's logout prompt confirms itself when its countdown ends, and its shutdown service asks
logind, on the machine's system bus, to restart or power off the computer. It prints "ready" once it owns every name,
or exits 1 when one is already taken.
"""
import sys

from gi.repository import Gio, GLib

LOG = sys.argv[1]
SERVICES = {
    "org.kde.LogoutPrompt": ("/LogoutPrompt", "org.kde.LogoutPrompt", ["promptLogout", "promptShutDown", "promptReboot"]),
    "org.kde.Shutdown": ("/Shutdown", "org.kde.Shutdown", ["logout", "logoutAndShutdown", "logoutAndReboot"]),
    "org.freedesktop.ScreenSaver": ("/ScreenSaver", "org.freedesktop.ScreenSaver", ["Lock"]),
}


def interface_xml(name, methods):
    calls = "".join(f'<method name="{method}"/>' for method in methods)
    return f'<node><interface name="{name}">{calls}</interface></node>'


def on_call(connection, sender, path, interface, method, parameters, invocation):
    with open(LOG, "a") as f:
        f.write(f"{interface}.{method}\n")
    invocation.return_value(None)


def main():
    bus = Gio.bus_get_sync(Gio.BusType.SESSION)
    for name, (path, interface, methods) in SERVICES.items():
        info = Gio.DBusNodeInfo.new_for_xml(interface_xml(interface, methods)).interfaces[0]
        bus.register_object(path, info, on_call, None, None)
        # DO_NOT_QUEUE: when the name is taken, fail instead of waiting behind the real service.
        reply = bus.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
                              GLib.Variant("(su)", (name, 4)), GLib.VariantType("(u)"), Gio.DBusCallFlags.NONE, -1, None)
        if reply.unpack()[0] != 1:  # 1: we are the primary owner
            print(f"{name} is already owned", flush=True)
            sys.exit(1)
    print("ready", flush=True)
    GLib.MainLoop().run()


if __name__ == "__main__":
    main()
