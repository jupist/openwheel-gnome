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
 * Executes actions by simulating keyboard and mouse events
 */
class ActionExecutor : public QObject
{
    Q_OBJECT

public:
    explicit ActionExecutor(QObject *parent = nullptr);
    ~ActionExecutor() override;

    /**
     * Execute an action configuration
     */
    void executeAction(const ActionConfig &action, int repeatCount = 1);

    /**
     * Execute a keyboard press (and release)
     */
    void executeKeyPress(const QString &keys, Qt::KeyboardModifiers modifiers = Qt::NoModifier);

    /**
     * Execute mouse scroll
     */
    void executeMouseScroll(int delta, Qt::Orientation orientation = Qt::Vertical);

    /**
     * Execute D-Bus call
     */
    void executeDBusCall(const ActionConfig &action);

    /**
     * Execute shell command
     */
    void executeCommand(const QString &command);

private:
    void initializeX11();
    void cleanupX11();

#ifdef HAVE_X11
    void sendKeyX11(const QString &keys, Qt::KeyboardModifiers modifiers, bool press);
    void sendModifiersX11(Qt::KeyboardModifiers modifiers, bool press);
    void sendMouseScrollX11(int delta, Qt::Orientation orientation);
    KeySym qtKeyToX11Keysym(const QString &key);
    Qt::Key parseKeyString(const QString &keyStr);
#endif

    bool m_x11Available = false;

#ifdef HAVE_X11
    Display *m_display = nullptr;
    static QHash<QString, KeySym> s_keySymMap;
#endif
};
