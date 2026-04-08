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

    // Release any modifier keys held by a sticky-modifier action.
    // Must be called by DialController when the dial button is released.
    void releaseStickyModifiers();

Q_SIGNALS:
    void systemValueChanged(qreal value, qreal minValue, qreal maxValue);
    // Emitted just before a keystroke is injected that requires the overlay to
    // not hold Wayland keyboard focus (e.g. zoom Ctrl+=). The overlay should
    // briefly lower itself so the compositor can return focus to the user's app.
    void focusEscapeRequired();

private:
    void detectSession();

    // Wayland-native key handling via well-known D-Bus actions
    bool tryWaylandKeyAction(const QString &keys, Qt::KeyboardModifiers modifiers);
    void volumeChange(int direction);
    void brightnessChange(int direction);
    void zoomChange(int direction);

    // Brightness backends — return true on success. Tried in order based on
    // detected desktop environment.
    bool brightnessLogind(int direction);          // works on GNOME + KDE under logind
    bool brightnessGnomeSettingsDaemon(int direction);
    bool brightnessKdePowerDevil(int direction);
    bool brightnessctlSubprocess(int direction);

    // Helpers for sysfs backlight discovery (used by logind + brightnessctl).
    // Returns "" if no backlight device exists.
    QString discoverBacklightDevice();
    bool readBacklightValues(const QString &device, int *current, int *max);

    // Cached backlight device path component (e.g. "intel_backlight").
    // Filled in lazily on first brightness call. Empty string = none found.
    QString m_backlightDevice;
    bool m_backlightProbed = false;

    // Daemon-backed input injection (org.asus.dial.InjectKey/InjectScroll).
    // Returns true if the daemon is reachable AND the call succeeded.
    bool tryDaemonInjectKey(const QString &keys, Qt::KeyboardModifiers modifiers);
    bool tryDaemonInjectScroll(int delta, Qt::Orientation orientation);
    bool daemonInjectionAvailable();

    // Sticky modifier support — hold modifier keys across consecutive rotations.
    // executeStickyKeyPress() holds the required modifiers and taps the key.
    void executeStickyKeyPress(const QString &keys, Qt::KeyboardModifiers modifiers);
    bool tryDaemonHoldModifiers(quint32 owMods);
    bool tryDaemonReleaseModifiers(quint32 owMods);

    Qt::KeyboardModifiers m_stickyModsHeld = Qt::NoModifier;
    bool m_stickyActive = false;

    // Cached injection availability (-1 unknown, 0 no, 1 yes).
    int m_daemonInjectAvailable = -1;

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
