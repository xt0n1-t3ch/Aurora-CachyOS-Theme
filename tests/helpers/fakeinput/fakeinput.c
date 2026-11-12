// fakeinput: drives the pointer and keyboard of a KWin Wayland session through org_kde_kwin_fake_input,
// for the sandbox scenarios. Commands run in order:
//   move X Y          jump to X,Y (logical pixels)
//   glide X Y STEPS   move to X,Y in STEPS steps of one frame each, as a hand would
//   down | up         press or release the left button
//   rdown | rup       press or release the right button
//   mdown | mup       press or release the middle button
//   keydown CODE | keyup CODE   press or release an evdev key (125 is the left Windows key, 18 is E)
//   wait MS           pause for MS milliseconds
// The Wayland bindings come from fake-input.xml, generated with wayland-scanner. Build line: docs/build.md.
#include "fake-input-client-protocol.h"

#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>

static struct org_kde_kwin_fake_input *fake;

// Binds org_kde_kwin_fake_input when the compositor announces it, at version 4 or the compositor's own if lower.
static void on_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    (void)data;
    if (strcmp(interface, org_kde_kwin_fake_input_interface.name) == 0) {
        fake = wl_registry_bind(registry, name, &org_kde_kwin_fake_input_interface, version < 4 ? version : 4);
    }
}

// The registry listener requires this callback; nothing needs to react to a global going away.
static void on_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {on_global, on_global_remove};

// Sleeps for ms milliseconds.
static void sleep_ms(long ms)
{
    const struct timespec duration = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&duration, NULL);
}

// Moves the pointer to the absolute position x,y in logical pixels.
static void move_to(double x, double y)
{
    org_kde_kwin_fake_input_pointer_motion_absolute(fake, wl_fixed_from_double(x), wl_fixed_from_double(y));
}

// Runs the commands in argv in order. Returns 0 when all ran, 1 when there is no Wayland display or the compositor
// lacks org_kde_kwin_fake_input, and 2 on an unknown command (the commands before it have already run).
int main(int argc, char **argv)
{
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "fakeinput: no Wayland display\n");
        return 1;
    }
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);
    if (!fake) {
        fprintf(stderr, "fakeinput: the compositor offers no org_kde_kwin_fake_input\n");
        return 1;
    }
    org_kde_kwin_fake_input_authenticate(fake, "aurora-sandbox", "screenshot scenarios");

    double x = 0;
    double y = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "move") == 0 && i + 2 < argc) {
            x = atof(argv[++i]);
            y = atof(argv[++i]);
            move_to(x, y);
        } else if (strcmp(argv[i], "glide") == 0 && i + 3 < argc) {
            const double toX = atof(argv[++i]);
            const double toY = atof(argv[++i]);
            const int steps = atoi(argv[++i]);
            for (int step = 1; step <= steps; ++step) {
                move_to(x + (toX - x) * step / steps, y + (toY - y) * step / steps);
                wl_display_flush(display);
                sleep_ms(16);
            }
            x = toX;
            y = toY;
        } else if (strcmp(argv[i], "down") == 0) {
            org_kde_kwin_fake_input_button(fake, BTN_LEFT, 1);
        } else if (strcmp(argv[i], "up") == 0) {
            org_kde_kwin_fake_input_button(fake, BTN_LEFT, 0);
        } else if (strcmp(argv[i], "rdown") == 0) {
            org_kde_kwin_fake_input_button(fake, BTN_RIGHT, 1);
        } else if (strcmp(argv[i], "rup") == 0) {
            org_kde_kwin_fake_input_button(fake, BTN_RIGHT, 0);
        } else if (strcmp(argv[i], "mdown") == 0) {
            org_kde_kwin_fake_input_button(fake, BTN_MIDDLE, 1);
        } else if (strcmp(argv[i], "mup") == 0) {
            org_kde_kwin_fake_input_button(fake, BTN_MIDDLE, 0);
        } else if (strcmp(argv[i], "keydown") == 0 && i + 1 < argc) {
            org_kde_kwin_fake_input_keyboard_key(fake, (uint32_t)atoi(argv[++i]), 1);
        } else if (strcmp(argv[i], "keyup") == 0 && i + 1 < argc) {
            org_kde_kwin_fake_input_keyboard_key(fake, (uint32_t)atoi(argv[++i]), 0);
        } else if (strcmp(argv[i], "wait") == 0 && i + 1 < argc) {
            wl_display_flush(display);
            sleep_ms(atol(argv[++i]));
        } else {
            fprintf(stderr, "fakeinput: unknown command %s\n", argv[i]);
            return 2;
        }
        wl_display_flush(display);
    }
    wl_display_roundtrip(display);
    wl_display_disconnect(display);
    return 0;
}
