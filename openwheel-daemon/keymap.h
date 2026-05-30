// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors
//
// Mapping from human-readable key names (as stored in profile JSON) to
// Linux input event codes (KEY_*). Used by the uinput injector and exposed
// to clients via the org.asus.dial.ListSupportedKeys D-Bus method.

#ifndef OPENWHEEL_KEYMAP_H
#define OPENWHEEL_KEYMAP_H

#include <stddef.h>

// Modifier bitmask used by InjectKey D-Bus method.
// These values must stay in sync with the gadget's ActionExecutor.
#define OW_MOD_CTRL   0x01u
#define OW_MOD_SHIFT  0x02u
#define OW_MOD_ALT    0x04u
#define OW_MOD_SUPER  0x08u

// Returns the Linux KEY_* code for `name`, or -1 if not found.
// Lookup is case-insensitive.
int keymap_lookup(const char *name);

// Returns total number of supported key names (for ListSupportedKeys).
size_t keymap_count(void);

// Returns the canonical name at index `i`, or NULL if out of range.
const char *keymap_name_at(size_t i);

// Returns the Linux KEY_* code at index `i`, or -1 if out of range.
int keymap_code_at(size_t i);

#endif // OPENWHEEL_KEYMAP_H
