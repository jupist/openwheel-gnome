// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#pragma once

#include "../profiles/profile.h"
#include <QObject>
#include <QHash>

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

/**
 * Executes actions by simulating keyboard and mouse events.
 * Prefers Wayland-native mechanisms (D-Bus, wpctl, ydotool)
 * and falls back to X11/XTest when running under X11.
 */
class ActionExecutor : public QObject
{
    Q_OBJECT

public:
    explicit ActionExecutor(QObject *parent = nullptr);
    ~ActionExecutor() override;

    void executeAction(const ActionConfig &action, int repeatCount = 1);
    void executeKeyPress(const QString &keys, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void executeMouseScroll(int delta, Qt::Orientation orientation = Qt::Vertical);
    void executeDBusCall(const ActionConfig &action);
    void executeCommand(const QString &command);
    void queryCurrentValue(const QString &keys);

Q_SIGNALS:
    void systemValueChanged(qreal value, qreal minValue, qreal maxValue);

private:
    void detectSession();

    // Wayland-native key handling via well-known D-Bus actions
    bool tryWaylandKeyAction(const QString &keys, Qt::KeyboardModifiers modifiers);
    void volumeChange(int direction);
    void brightnessChange(int direction);
    void zoomChange(int direction);

#ifdef HAVE_X11
    void initializeX11();
    void cleanupX11();
    void sendKeyX11(const QString &keys, Qt::KeyboardModifiers modifiers, int press);
    void sendModifiersX11(Qt::KeyboardModifiers modifiers, int press);
    void sendMouseScrollX11(int delta, Qt::Orientation orientation);
    KeySym qtKeyToX11Keysym(const QString &key);

    Display *m_display = nullptr;
    static QHash<QString, KeySym> s_keySymMap;
#endif

    bool m_isWayland = false;
    int m_x11Available = 0;
};
