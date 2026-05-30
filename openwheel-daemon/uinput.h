// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors
//
// Thin wrapper around /dev/uinput. Creates one virtual device that exposes
// a keyboard, mouse buttons, and a relative scroll wheel. Used by the
// openwheel daemon to inject synthetic input events from D-Bus method
// calls — primarily so the gadget has a Wayland-compatible way to send
// keystrokes and scroll events to the focused window.

#ifndef OPENWHEEL_UINPUT_H
#define OPENWHEEL_UINPUT_H

#include <stdint.h>

// Initialise the virtual device. Returns 0 on success, -1 on failure
// (typically EACCES on /dev/uinput — see udev rules in the install).
int uinput_init(void);

// Tear down the virtual device. Safe to call even if init failed.
void uinput_cleanup(void);

// Reports whether the device was successfully created.
int uinput_available(void);

// Tap a key (press + release) with the given modifier bitmask
// (see OW_MOD_* in keymap.h). Returns 0 on success.
int uinput_tap_key(int code, uint32_t modifiers);

// Send `clicks` worth of vertical scroll. Positive = up. Returns 0 on success.
int uinput_scroll_vertical(int clicks);

// Send `clicks` worth of horizontal scroll. Positive = right. Returns 0 on success.
int uinput_scroll_horizontal(int clicks);

// Tap a mouse button (BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, ...).
// Use raw Linux BTN_* codes. Returns 0 on success.
int uinput_tap_button(int btn_code);

// Hold a set of modifier keys down without releasing them.
// Use OW_MOD_* bitmask from keymap.h. Returns 0 on success.
// Caller MUST eventually call uinput_release_modifiers() with the same mask.
int uinput_hold_modifiers(uint32_t modifiers);

// Release modifier keys that were held by uinput_hold_modifiers().
// Use OW_MOD_* bitmask from keymap.h. Returns 0 on success.
int uinput_release_modifiers(uint32_t modifiers);

#endif // OPENWHEEL_UINPUT_H
