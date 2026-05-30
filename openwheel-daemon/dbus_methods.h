// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors
//
// Registers the org.asus.dial D-Bus object that exposes input-injection
// methods (InjectKey, InjectScroll, InjectButton, ListSupportedKeys).
// The connection is the same one used to broadcast the dial's Rotate /
// Press signals from hidreader.c.

#ifndef OPENWHEEL_DBUS_METHODS_H
#define OPENWHEEL_DBUS_METHODS_H

#include <dbus/dbus.h>

// Register /org/asus/dial as a method handler on `connection`.
// Returns 0 on success, -1 on failure.
int dbus_methods_register(DBusConnection *connection);

// Process any pending D-Bus messages without blocking. Call this from
// the main loop in between HID reads. Returns the number of messages
// dispatched.
int dbus_methods_dispatch(DBusConnection *connection);

#endif // OPENWHEEL_DBUS_METHODS_H
