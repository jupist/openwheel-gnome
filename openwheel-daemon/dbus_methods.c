// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors

#include "dbus_methods.h"
#include "keymap.h"
#include "uinput.h"

#include <dbus/dbus.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#define OW_OBJECT_PATH "/org/asus/dial"
#define OW_INTERFACE   "org.asus.dial"

// Introspection XML — describes the methods so `gdbus introspect` and
// language bindings can discover them.
static const char *kIntrospectionXml =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    " \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
    "    <method name=\"Introspect\">\n"
    "      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "  <interface name=\"org.asus.dial\">\n"
    "    <signal name=\"Rotate\">\n"
    "      <arg name=\"delta\" type=\"i\"/>\n"
    "    </signal>\n"
    "    <signal name=\"Press\">\n"
    "      <arg name=\"state\" type=\"i\"/>\n"
    "    </signal>\n"
    "    <method name=\"InjectKey\">\n"
    "      <arg name=\"key_name\" direction=\"in\" type=\"s\"/>\n"
    "      <arg name=\"modifiers\" direction=\"in\" type=\"u\"/>\n"
    "      <arg name=\"success\" direction=\"out\" type=\"b\"/>\n"
    "    </method>\n"
    "    <method name=\"InjectScroll\">\n"
    "      <arg name=\"clicks\" direction=\"in\" type=\"i\"/>\n"
    "      <arg name=\"horizontal\" direction=\"in\" type=\"b\"/>\n"
    "      <arg name=\"success\" direction=\"out\" type=\"b\"/>\n"
    "    </method>\n"
    "    <method name=\"InjectButton\">\n"
    "      <arg name=\"button_code\" direction=\"in\" type=\"u\"/>\n"
    "      <arg name=\"success\" direction=\"out\" type=\"b\"/>\n"
    "    </method>\n"
    "    <method name=\"ListSupportedKeys\">\n"
    "      <arg name=\"keys\" direction=\"out\" type=\"as\"/>\n"
    "    </method>\n"
    "    <method name=\"InjectionAvailable\">\n"
    "      <arg name=\"available\" direction=\"out\" type=\"b\"/>\n"
    "    </method>\n"
    "    <method name=\"HoldModifiers\">\n"
    "      <arg name=\"modifiers\" direction=\"in\" type=\"u\"/>\n"
    "      <arg name=\"success\" direction=\"out\" type=\"b\"/>\n"
    "    </method>\n"
    "    <method name=\"ReleaseModifiers\">\n"
    "      <arg name=\"modifiers\" direction=\"in\" type=\"u\"/>\n"
    "      <arg name=\"success\" direction=\"out\" type=\"b\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

static void send_bool_reply(DBusConnection *conn, DBusMessage *msg, dbus_bool_t value)
{
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) return;
    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &value);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static void send_error_reply(DBusConnection *conn, DBusMessage *msg,
                             const char *err_name, const char *err_msg)
{
    DBusMessage *reply = dbus_message_new_error(msg, err_name, err_msg);
    if (!reply) return;
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
}

static DBusHandlerResult handle_introspect(DBusConnection *conn, DBusMessage *msg)
{
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) return DBUS_HANDLER_RESULT_NEED_MEMORY;

    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);
    const char *xml = kIntrospectionXml;
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &xml);

    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_inject_key(DBusConnection *conn, DBusMessage *msg)
{
    DBusError err;
    dbus_error_init(&err);

    const char *key_name = NULL;
    dbus_uint32_t modifiers = 0;

    if (!dbus_message_get_args(msg, &err,
                               DBUS_TYPE_STRING, &key_name,
                               DBUS_TYPE_UINT32, &modifiers,
                               DBUS_TYPE_INVALID)) {
        send_error_reply(conn, msg, DBUS_ERROR_INVALID_ARGS,
                         err.message ? err.message : "Expected (s, u)");
        dbus_error_free(&err);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    int code = keymap_lookup(key_name);
    if (code < 0) {
        syslog(LOG_INFO, "InjectKey: unknown key '%s'", key_name);
        send_bool_reply(conn, msg, FALSE);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    int rc = uinput_tap_key(code, modifiers);
    syslog(LOG_DEBUG, "InjectKey: '%s' (code=%d mods=0x%x) -> %d",
           key_name, code, modifiers, rc);
    send_bool_reply(conn, msg, rc == 0 ? TRUE : FALSE);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_inject_scroll(DBusConnection *conn, DBusMessage *msg)
{
    DBusError err;
    dbus_error_init(&err);

    dbus_int32_t clicks = 0;
    dbus_bool_t  horizontal = FALSE;

    if (!dbus_message_get_args(msg, &err,
                               DBUS_TYPE_INT32, &clicks,
                               DBUS_TYPE_BOOLEAN, &horizontal,
                               DBUS_TYPE_INVALID)) {
        send_error_reply(conn, msg, DBUS_ERROR_INVALID_ARGS,
                         err.message ? err.message : "Expected (i, b)");
        dbus_error_free(&err);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    int rc = horizontal ? uinput_scroll_horizontal(clicks)
                        : uinput_scroll_vertical(clicks);
    syslog(LOG_DEBUG, "InjectScroll: clicks=%d horiz=%d -> %d", clicks, horizontal, rc);
    send_bool_reply(conn, msg, rc == 0 ? TRUE : FALSE);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_inject_button(DBusConnection *conn, DBusMessage *msg)
{
    DBusError err;
    dbus_error_init(&err);

    dbus_uint32_t btn = 0;

    if (!dbus_message_get_args(msg, &err,
                               DBUS_TYPE_UINT32, &btn,
                               DBUS_TYPE_INVALID)) {
        send_error_reply(conn, msg, DBUS_ERROR_INVALID_ARGS,
                         err.message ? err.message : "Expected (u)");
        dbus_error_free(&err);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    int rc = uinput_tap_button((int)btn);
    syslog(LOG_DEBUG, "InjectButton: code=%u -> %d", btn, rc);
    send_bool_reply(conn, msg, rc == 0 ? TRUE : FALSE);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_list_keys(DBusConnection *conn, DBusMessage *msg)
{
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) return DBUS_HANDLER_RESULT_NEED_MEMORY;

    DBusMessageIter iter, arr;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &arr);
    for (size_t i = 0; i < keymap_count(); ++i) {
        const char *name = keymap_name_at(i);
        if (name) {
            dbus_message_iter_append_basic(&arr, DBUS_TYPE_STRING, &name);
        }
    }
    dbus_message_iter_close_container(&iter, &arr);

    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_injection_available(DBusConnection *conn, DBusMessage *msg)
{
    send_bool_reply(conn, msg, uinput_available() ? TRUE : FALSE);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_hold_modifiers(DBusConnection *conn, DBusMessage *msg)
{
    DBusError err;
    dbus_error_init(&err);
    dbus_uint32_t modifiers = 0;
    if (!dbus_message_get_args(msg, &err,
                               DBUS_TYPE_UINT32, &modifiers,
                               DBUS_TYPE_INVALID)) {
        send_error_reply(conn, msg, DBUS_ERROR_INVALID_ARGS,
                         err.message ? err.message : "Expected (u)");
        dbus_error_free(&err);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    int rc = uinput_hold_modifiers(modifiers);
    syslog(LOG_DEBUG, "HoldModifiers: mods=0x%x -> %d", modifiers, rc);
    send_bool_reply(conn, msg, rc == 0 ? TRUE : FALSE);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_release_modifiers(DBusConnection *conn, DBusMessage *msg)
{
    DBusError err;
    dbus_error_init(&err);
    dbus_uint32_t modifiers = 0;
    if (!dbus_message_get_args(msg, &err,
                               DBUS_TYPE_UINT32, &modifiers,
                               DBUS_TYPE_INVALID)) {
        send_error_reply(conn, msg, DBUS_ERROR_INVALID_ARGS,
                         err.message ? err.message : "Expected (u)");
        dbus_error_free(&err);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    int rc = uinput_release_modifiers(modifiers);
    syslog(LOG_DEBUG, "ReleaseModifiers: mods=0x%x -> %d", modifiers, rc);
    send_bool_reply(conn, msg, rc == 0 ? TRUE : FALSE);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult message_handler(DBusConnection *conn,
                                         DBusMessage *msg,
                                         void *user_data)
{
    (void)user_data;

    if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
        return handle_introspect(conn, msg);
    }
    if (dbus_message_is_method_call(msg, OW_INTERFACE, "InjectKey")) {
        return handle_inject_key(conn, msg);
    }
    if (dbus_message_is_method_call(msg, OW_INTERFACE, "InjectScroll")) {
        return handle_inject_scroll(conn, msg);
    }
    if (dbus_message_is_method_call(msg, OW_INTERFACE, "InjectButton")) {
        return handle_inject_button(conn, msg);
    }
    if (dbus_message_is_method_call(msg, OW_INTERFACE, "ListSupportedKeys")) {
        return handle_list_keys(conn, msg);
    }
    if (dbus_message_is_method_call(msg, OW_INTERFACE, "InjectionAvailable")) {
        return handle_injection_available(conn, msg);
    }
    if (dbus_message_is_method_call(msg, OW_INTERFACE, "HoldModifiers")) {
        return handle_hold_modifiers(conn, msg);
    }
    if (dbus_message_is_method_call(msg, OW_INTERFACE, "ReleaseModifiers")) {
        return handle_release_modifiers(conn, msg);
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static const DBusObjectPathVTable kVtable = {
    .unregister_function = NULL,
    .message_function = message_handler,
};

int dbus_methods_register(DBusConnection *connection)
{
    if (!connection) return -1;
    if (!dbus_connection_register_object_path(connection, OW_OBJECT_PATH, &kVtable, NULL)) {
        syslog(LOG_ERR, "dbus: failed to register object path %s", OW_OBJECT_PATH);
        return -1;
    }
    syslog(LOG_INFO, "dbus: registered %s on %s", OW_INTERFACE, OW_OBJECT_PATH);
    return 0;
}

int dbus_methods_dispatch(DBusConnection *connection)
{
    if (!connection) return 0;
    int dispatched = 0;
    // Pump pending I/O without blocking, then dispatch any complete messages.
    dbus_connection_read_write(connection, 0);
    while (dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS) {
        ++dispatched;
    }
    return dispatched;
}
