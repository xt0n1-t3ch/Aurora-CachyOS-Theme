#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""A tiny MPRIS player for sandbox captures: it plays "Emerald Bay" by "Tahoe Sessions" with an artwork file,
so the Control Center's Now Playing module has something real to show. Runs until killed."""
import pathlib
import sys

import gi

gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib  # noqa: E402

ART = pathlib.Path(__file__).resolve().parent / "fake-album.png"
XML = """
<node>
  <interface name="org.mpris.MediaPlayer2">
    <property name="Identity" type="s" access="read"/>
    <property name="CanRaise" type="b" access="read"/>
    <property name="CanQuit" type="b" access="read"/>
    <property name="DesktopEntry" type="s" access="read"/>
    <method name="Raise"/>
    <method name="Quit"/>
  </interface>
  <interface name="org.mpris.MediaPlayer2.Player">
    <property name="PlaybackStatus" type="s" access="read"/>
    <property name="Metadata" type="a{sv}" access="read"/>
    <property name="CanControl" type="b" access="read"/>
    <property name="CanPlay" type="b" access="read"/>
    <property name="CanPause" type="b" access="read"/>
    <property name="CanGoNext" type="b" access="read"/>
    <property name="CanGoPrevious" type="b" access="read"/>
    <property name="CanSeek" type="b" access="read"/>
    <property name="Position" type="x" access="read"/>
    <property name="Volume" type="d" access="readwrite"/>
    <property name="Rate" type="d" access="read"/>
    <method name="PlayPause"/>
    <method name="Play"/>
    <method name="Pause"/>
    <method name="Next"/>
    <method name="Previous"/>
  </interface>
</node>
"""


def metadata():
    """Return the Metadata property: the track's id, title, artist, length and artwork URL."""
    return {
        "mpris:trackid": GLib.Variant("o", "/org/mpris/MediaPlayer2/track/1"),
        "xesam:title": GLib.Variant("s", "Emerald Bay"),
        "xesam:artist": GLib.Variant("as", ["Tahoe Sessions"]),
        "mpris:length": GLib.Variant("x", 214_000_000),
        "mpris:artUrl": GLib.Variant("s", ART.as_uri() if ART.exists() else ""),
    }


# Every property except Metadata, which metadata() builds on each read.
PROPS = {
    "Identity": GLib.Variant("s", "Sandbox Player"),
    "CanRaise": GLib.Variant("b", False),
    "CanQuit": GLib.Variant("b", False),
    "DesktopEntry": GLib.Variant("s", ""),
    "PlaybackStatus": GLib.Variant("s", "Playing"),
    "CanControl": GLib.Variant("b", True),
    "CanPlay": GLib.Variant("b", True),
    "CanPause": GLib.Variant("b", True),
    "CanGoNext": GLib.Variant("b", True),
    "CanGoPrevious": GLib.Variant("b", True),
    "CanSeek": GLib.Variant("b", False),
    "Position": GLib.Variant("x", 61_000_000),
    "Volume": GLib.Variant("d", 1.0),
    "Rate": GLib.Variant("d", 1.0),
}


def on_call(connection, sender, path, interface, method, params, invocation):
    """Answer every method call (Raise, Quit, PlayPause and the rest) with an empty reply and do nothing else."""
    invocation.return_value(None)


def on_get(connection, sender, path, interface, name):
    """Return the value of the property name on either MPRIS interface, or None for an unknown one."""
    if name == "Metadata":
        return GLib.Variant("a{sv}", metadata())
    return PROPS.get(name)


def main():
    """Own org.mpris.MediaPlayer2.sandboxplayer on the session bus and serve the player until killed."""
    node = Gio.DBusNodeInfo.new_for_xml(XML)
    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    for interface in node.interfaces:
        bus.register_object("/org/mpris/MediaPlayer2", interface, on_call, on_get, None)
    Gio.bus_own_name_on_connection(bus, "org.mpris.MediaPlayer2.sandboxplayer", Gio.BusNameOwnerFlags.NONE, None, None)
    GLib.MainLoop().run()


if __name__ == "__main__":
    sys.exit(main())
