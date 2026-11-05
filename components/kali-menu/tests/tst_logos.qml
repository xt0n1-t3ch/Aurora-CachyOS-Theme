// SPDX-License-Identifier: GPL-2.0-or-later
// The menu bar logo the settings page offers (package/contents/ui/code/tools.js): Aurora's logos, the default, and
// how a saved icon name maps back to a logo and its colors. Run with: qmltestrunner -input tests/tst_logos.qml
import QtQuick
import QtTest
import "../package/contents/ui/code/tools.js" as Tools

TestCase {
    name: "MenuLogos"

    // The default is CachyOS's black-and-white logo, so it matches the rest of the menu bar (docs/decisions.md).
    function test_the_default_logo_is_cachyos_in_black_and_white() {
        compare(Tools.defaultIconName, "aurora-logo-cachyos-symbolic");
        compare(Tools.logoOf(Tools.defaultIconName), {id: "cachyos", colored: false});
    }

    function test_the_logos_in_the_order_the_settings_show_them() {
        compare(Tools.logos.map(logo => logo.id), ["cachyos", "arch", "apple", "kali"]);
        compare(Tools.logos.map(logo => logo.name), ["CachyOS", "Arch Linux", "Apple", "Kali Linux"]);
    }

    function test_a_logo_with_colors_gives_its_colored_icon_when_asked() {
        compare(Tools.logoIcon("cachyos", true), "aurora-logo-cachyos");
        compare(Tools.logoIcon("cachyos", false), "aurora-logo-cachyos-symbolic");
        compare(Tools.logoIcon("arch", true), "aurora-logo-arch");
    }

    function test_a_logo_without_colors_stays_white() {
        compare(Tools.logoIcon("apple", true), "aurora-logo-apple-symbolic");
        compare(Tools.logoIcon("kali", false), "aurora-logo-kali-symbolic");
        verify(!Tools.hasColors("apple"));
        verify(Tools.hasColors("arch"));
    }

    function test_a_saved_icon_maps_back_to_its_logo() {
        compare(Tools.logoOf("aurora-logo-arch-symbolic"), {id: "arch", colored: false});
        compare(Tools.logoOf("aurora-logo-kali-symbolic"), {id: "kali", colored: false});
    }

    function test_any_other_icon_is_a_custom_one() {
        compare(Tools.logoOf("start-here-kde-symbolic"), null);
        compare(Tools.logoOf(""), null);
        compare(Tools.logoIcon("no-such-logo", true), "");
    }
}
