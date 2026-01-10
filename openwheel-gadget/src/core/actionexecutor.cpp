// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include "actionexecutor.h"
#include <QProcess>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>

#ifdef HAVE_X11
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <QX11Info>
#endif

#ifdef HAVE_X11
QHash<QString, KeySym> ActionExecutor::s_keySymMap;
#endif

ActionExecutor::ActionExecutor(QObject *parent)
    : QObject(parent)
{
#ifdef HAVE_X11
    initializeX11();
#endif
}

ActionExecutor::~ActionExecutor()
{
#ifdef HAVE_X11
    cleanupX11();
#endif
}

void ActionExecutor::executeAction(const ActionConfig &action, int repeatCount)
{
    for (int i = 0; i < repeatCount; ++i) {
        switch (action.type) {
            case ActionConfig::Type::Keyboard:
            case ActionConfig::Type::KeyboardRepeat:
                executeKeyPress(action.keys, action.modifiers);
                break;

            case ActionConfig::Type::MouseScroll:
                executeMouseScroll(action.delta, action.orientation);
                break;

            case ActionConfig::Type::DBusCall:
                executeDBusCall(action);
                break;

            case ActionConfig::Type::Command:
                executeCommand(action.command);
                break;

            default:
                qWarning() << "Unknown action type";
                break;
        }

        // Small delay between repeats
        if (i < repeatCount - 1 && repeatCount > 1) {
            QThread::msleep(10);
        }
    }
}

void ActionExecutor::executeKeyPress(const QString &keys, Qt::KeyboardModifiers modifiers)
{
    if (keys.isEmpty()) {
        return;
    }

#ifdef HAVE_X11
    if (m_x11Available) {
        sendKeyX11(keys, modifiers, true);   // Press
        QThread::msleep(20);                 // Small delay
        sendKeyX11(keys, modifiers, false);  // Release
        XFlush(m_display);
    } else
#endif
    {
        qWarning() << "Keyboard simulation not available (X11 required)";
    }
}

void ActionExecutor::executeMouseScroll(int delta, Qt::Orientation orientation)
{
#ifdef HAVE_X11
    if (m_x11Available) {
        sendMouseScrollX11(delta, orientation);
        XFlush(m_display);
    } else
#endif
    {
        qWarning() << "Mouse simulation not available (X11 required)";
    }
}

void ActionExecutor::executeDBusCall(const ActionConfig &action)
{
    if (action.dbusService.isEmpty() || action.dbusMethod.isEmpty()) {
        qWarning() << "Invalid D-Bus action configuration";
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        action.dbusService,
        action.dbusPath,
        action.dbusInterface,
        action.dbusMethod
    );

    message.setArguments(action.dbusArgs);

    QDBusConnection::sessionBus().call(message, QDBus::NoBlock);

    qDebug() << "D-Bus call:" << action.dbusService << action.dbusMethod;
}

void ActionExecutor::executeCommand(const QString &command)
{
    if (command.isEmpty()) {
        return;
    }

    qDebug() << "Executing command:" << command;
    QProcess::startDetached(QStringLiteral("/bin/sh"), QStringList() << QStringLiteral("-c") << command);
}

#ifdef HAVE_X11
void ActionExecutor::initializeX11()
{
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        qWarning() << "Failed to open X11 display";
        m_x11Available = false;
        return;
    }

    // Check for XTest extension
    int event_base, error_base, major, minor;
    if (!XTestQueryExtension(m_display, &event_base, &error_base, &major, &minor)) {
        qWarning() << "XTest extension not available";
        XCloseDisplay(m_display);
        m_display = nullptr;
        m_x11Available = false;
        return;
    }

    m_x11Available = true;
    qDebug() << "X11 input simulation initialized";

    // Initialize key symbol map
    if (s_keySymMap.isEmpty()) {
        s_keySymMap[QStringLiteral("bracketleft")] = XK_bracketleft;
        s_keySymMap[QStringLiteral("[")] = XK_bracketleft;
        s_keySymMap[QStringLiteral("bracketright")] = XK_bracketright;
        s_keySymMap[QStringLiteral("]")] = XK_bracketright;
        s_keySymMap[QStringLiteral("plus")] = XK_plus;
        s_keySymMap[QStringLiteral("+")] = XK_plus;
        s_keySymMap[QStringLiteral("minus")] = XK_minus;
        s_keySymMap[QStringLiteral("-")] = XK_minus;
        s_keySymMap[QStringLiteral("equal")] = XK_equal;
        s_keySymMap[QStringLiteral("=")] = XK_equal;
        s_keySymMap[QStringLiteral("comma")] = XK_comma;
        s_keySymMap[QStringLiteral(",")] = XK_comma;
        s_keySymMap[QStringLiteral("period")] = XK_period;
        s_keySymMap[QStringLiteral(".")] = XK_period;
        s_keySymMap[QStringLiteral("space")] = XK_space;
        s_keySymMap[QStringLiteral(" ")] = XK_space;
        s_keySymMap[QStringLiteral("return")] = XK_Return;
        s_keySymMap[QStringLiteral("enter")] = XK_Return;
        s_keySymMap[QStringLiteral("left")] = XK_Left;
        s_keySymMap[QStringLiteral("right")] = XK_Right;
        s_keySymMap[QStringLiteral("up")] = XK_Up;
        s_keySymMap[QStringLiteral("down")] = XK_Down;
        s_keySymMap[QStringLiteral("z")] = XK_z;
        s_keySymMap[QStringLiteral("j")] = XK_j;
        s_keySymMap[QStringLiteral("k")] = XK_k;
        s_keySymMap[QStringLiteral("l")] = XK_l;
        s_keySymMap[QStringLiteral("i")] = XK_i;
        s_keySymMap[QStringLiteral("o")] = XK_o;
        s_keySymMap[QStringLiteral("4")] = XK_4;
        s_keySymMap[QStringLiteral("6")] = XK_6;
        s_keySymMap[QStringLiteral("a")] = XK_a;
        s_keySymMap[QStringLiteral("b")] = XK_b;
        s_keySymMap[QStringLiteral("c")] = XK_c;
        // Add more as needed...
    }
}

void ActionExecutor::cleanupX11()
{
    if (m_display) {
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
}

void ActionExecutor::sendKeyX11(const QString &keys, Qt::KeyboardModifiers modifiers, bool press)
{
    if (!m_display) {
        return;
    }

    // Send modifiers first if pressing
    if (press) {
        sendModifiersX11(modifiers, true);
        QThread::msleep(10);
    }

    // Send main key
    KeySym keysym = qtKeyToX11Keysym(keys);
    if (keysym != NoSymbol) {
        KeyCode keycode = XKeysymToKeycode(m_display, keysym);
        if (keycode != 0) {
            XTestFakeKeyEvent(m_display, keycode, press ? True : False, CurrentTime);
        } else {
            qWarning() << "No keycode found for keysym:" << keys;
        }
    } else {
        qWarning() << "No keysym found for key:" << keys;
    }

    // Release modifiers if releasing
    if (!press) {
        QThread::msleep(10);
        sendModifiersX11(modifiers, false);
    }
}

void ActionExecutor::sendModifiersX11(Qt::KeyboardModifiers modifiers, bool press)
{
    if (!m_display) {
        return;
    }

    struct ModifierKey {
        Qt::KeyboardModifier mod;
        KeySym keysym;
    };

    static const ModifierKey modKeys[] = {
        { Qt::ControlModifier, XK_Control_L },
        { Qt::ShiftModifier, XK_Shift_L },
        { Qt::AltModifier, XK_Alt_L },
        { Qt::MetaModifier, XK_Super_L }
    };

    for (const auto &modKey : modKeys) {
        if (modifiers & modKey.mod) {
            KeyCode keycode = XKeysymToKeycode(m_display, modKey.keysym);
            if (keycode != 0) {
                XTestFakeKeyEvent(m_display, keycode, press ? True : False, CurrentTime);
            }
        }
    }
}

void ActionExecutor::sendMouseScrollX11(int delta, Qt::Orientation orientation)
{
    if (!m_display) {
        return;
    }

    // X11 scroll wheel buttons: 4 = up, 5 = down, 6 = left, 7 = right
    unsigned int button;

    if (orientation == Qt::Vertical) {
        button = (delta > 0) ? 4 : 5;
    } else {
        button = (delta > 0) ? 7 : 6;
    }

    int clicks = qAbs(delta);
    for (int i = 0; i < clicks; ++i) {
        XTestFakeButtonEvent(m_display, button, True, CurrentTime);
        XTestFakeButtonEvent(m_display, button, False, CurrentTime);
        if (i < clicks - 1) {
            QThread::msleep(10);
        }
    }
}

KeySym ActionExecutor::qtKeyToX11Keysym(const QString &key)
{
    QString lowerKey = key.toLower();

    // Check our map first
    if (s_keySymMap.contains(lowerKey)) {
        return s_keySymMap[lowerKey];
    }

    // For single characters, try XStringToKeysym
    if (key.length() == 1) {
        return XStringToKeysym(key.toLatin1().data());
    }

    return NoSymbol;
}
#endif
