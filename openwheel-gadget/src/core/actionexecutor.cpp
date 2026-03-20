// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include "actionexecutor.h"
#include <QProcess>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>
#include <QThread>

#ifdef HAVE_X11
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#endif

#ifdef HAVE_X11
QHash<QString, KeySym> ActionExecutor::s_keySymMap;
#endif

ActionExecutor::ActionExecutor(QObject *parent)
    : QObject(parent)
{
    detectSession();

#ifdef HAVE_X11
    if (!m_isWayland) {
        initializeX11();
    }
#endif
}

ActionExecutor::~ActionExecutor()
{
#ifdef HAVE_X11
    cleanupX11();
#endif
}

void ActionExecutor::detectSession()
{
    QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE");
    m_isWayland = (sessionType == QStringLiteral("wayland"));
    qDebug() << "Session type:" << sessionType << (m_isWayland ? "(Wayland)" : "(X11)");
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

    // On Wayland, try native D-Bus actions for well-known keys first
    if (m_isWayland && tryWaylandKeyAction(keys, modifiers)) {
        return;
    }

#ifdef HAVE_X11
    if (m_x11Available) {
        sendKeyX11(keys, modifiers, true);
        QThread::msleep(20);
        sendKeyX11(keys, modifiers, false);
        XFlush(m_display);
    } else
#endif
    {
        qWarning() << "Key simulation not available for:" << keys;
    }
}

bool ActionExecutor::tryWaylandKeyAction(const QString &keys, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);

    // Media keys → native D-Bus calls
    if (keys == QStringLiteral("XF86AudioRaiseVolume")) {
        volumeChange(+1);
        return true;
    }
    if (keys == QStringLiteral("XF86AudioLowerVolume")) {
        volumeChange(-1);
        return true;
    }
    if (keys == QStringLiteral("XF86AudioMute")) {
        QProcess::startDetached(QStringLiteral("wpctl"),
            {QStringLiteral("set-mute"), QStringLiteral("@DEFAULT_AUDIO_SINK@"), QStringLiteral("toggle")});
        return true;
    }
    if (keys == QStringLiteral("XF86MonBrightnessUp")) {
        brightnessChange(+1);
        return true;
    }
    if (keys == QStringLiteral("XF86MonBrightnessDown")) {
        brightnessChange(-1);
        return true;
    }

    if (keys == QStringLiteral("ZoomIn")) {
        zoomChange(+1);
        return true;
    }
    if (keys == QStringLiteral("ZoomOut")) {
        zoomChange(-1);
        return true;
    }

    return false;
}

void ActionExecutor::volumeChange(int direction)
{
    QString step = (direction > 0) ? QStringLiteral("5%+") : QStringLiteral("5%-");
    QProcess proc;
    proc.start(QStringLiteral("wpctl"),
        {QStringLiteral("set-volume"), QStringLiteral("-l"), QStringLiteral("1.0"),
         QStringLiteral("@DEFAULT_AUDIO_SINK@"), step});
    proc.waitForFinished(500);

    // Read back actual volume
    QProcess getVol;
    getVol.start(QStringLiteral("wpctl"),
        {QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@")});
    getVol.waitForFinished(500);
    QString output = QString::fromUtf8(getVol.readAllStandardOutput()).trimmed();
    // Format: "Volume: 0.75"
    QStringList parts = output.split(QLatin1Char(' '));
    if (parts.size() >= 2) {
        qreal vol = parts.last().toDouble() * 100.0;
        Q_EMIT systemValueChanged(vol, 0.0, 100.0);
        qDebug() << "Volume:" << vol << "%";
    }
}

void ActionExecutor::brightnessChange(int direction)
{
    // Use KDE PowerDevil D-Bus interface for brightness
    auto bus = QDBusConnection::sessionBus();
    QString service = QStringLiteral("org.kde.Solid.PowerManagement");
    QString path = QStringLiteral("/org/kde/Solid/PowerManagement/Actions/BrightnessControl");
    QString iface = QStringLiteral("org.kde.Solid.PowerManagement.Actions.BrightnessControl");

    // Get current and max brightness
    QDBusMessage getCurrent = QDBusMessage::createMethodCall(service, path, iface,
        QStringLiteral("brightness"));
    QDBusReply<int> currentReply = bus.call(getCurrent);

    QDBusMessage getMax = QDBusMessage::createMethodCall(service, path, iface,
        QStringLiteral("brightnessMax"));
    QDBusReply<int> maxReply = bus.call(getMax);

    if (!currentReply.isValid() || !maxReply.isValid()) {
        qWarning() << "Failed to get brightness via PowerDevil";
        return;
    }

    int current = currentReply.value();
    int max = maxReply.value();
    int step = qMax(1, max / 20);  // 5% steps
    int newBrightness = qBound(0, current + (direction * step), max);

    QDBusMessage setMsg = QDBusMessage::createMethodCall(service, path, iface,
        QStringLiteral("setBrightness"));
    setMsg << newBrightness;
    bus.call(setMsg, QDBus::NoBlock);

    qreal pct = (max > 0) ? (static_cast<qreal>(newBrightness) / max * 100.0) : 0.0;
    Q_EMIT systemValueChanged(pct, 0.0, 100.0);
    qDebug() << "Brightness:" << current << "->" << newBrightness << "/" << max;
}

void ActionExecutor::zoomChange(int direction)
{
    auto bus = QDBusConnection::sessionBus();
    QString shortcut = (direction > 0) ? QStringLiteral("view_zoom_in")
                                       : QStringLiteral("view_zoom_out");

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/component/kwin"),
        QStringLiteral("org.kde.kglobalaccel.Component"),
        QStringLiteral("invokeShortcut"));
    msg << shortcut;
    bus.call(msg, QDBus::NoBlock);

    qDebug() << "Zoom:" << shortcut;
}

void ActionExecutor::executeMouseScroll(int delta, Qt::Orientation orientation)
{
#ifdef HAVE_X11
    if (m_x11Available) {
        sendMouseScrollX11(delta, orientation);
        XFlush(m_display);
        return;
    }
#endif
    Q_UNUSED(delta);
    Q_UNUSED(orientation);
    qWarning() << "Mouse scroll simulation not available on Wayland without ydotool";
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

void ActionExecutor::queryCurrentValue(const QString &keys)
{
    if (keys == QStringLiteral("XF86AudioRaiseVolume") ||
        keys == QStringLiteral("XF86AudioLowerVolume")) {
        QProcess proc;
        proc.start(QStringLiteral("wpctl"),
            {QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@")});
        proc.waitForFinished(500);
        QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        QStringList parts = output.split(QLatin1Char(' '));
        if (parts.size() >= 2) {
            qreal vol = parts.last().toDouble() * 100.0;
            Q_EMIT systemValueChanged(vol, 0.0, 100.0);
        }
    } else if (keys == QStringLiteral("XF86MonBrightnessUp") ||
               keys == QStringLiteral("XF86MonBrightnessDown")) {
        auto bus = QDBusConnection::sessionBus();
        QString service = QStringLiteral("org.kde.Solid.PowerManagement");
        QString path = QStringLiteral("/org/kde/Solid/PowerManagement/Actions/BrightnessControl");
        QString iface = QStringLiteral("org.kde.Solid.PowerManagement.Actions.BrightnessControl");

        QDBusReply<int> current = bus.call(
            QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("brightness")));
        QDBusReply<int> max = bus.call(
            QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("brightnessMax")));

        if (current.isValid() && max.isValid() && max.value() > 0) {
            qreal pct = static_cast<qreal>(current.value()) / max.value() * 100.0;
            Q_EMIT systemValueChanged(pct, 0.0, 100.0);
        }
    }
    // Zoom and Scroll have no queryable value — no action needed
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

    int event_base, error_base, major, minor;
    if (!XTestQueryExtension(m_display, &event_base, &error_base, &major, &minor)) {
        qWarning() << "XTest extension not available";
        XCloseDisplay(m_display);
        m_display = nullptr;
        m_x11Available = false;
        return;
    }

    m_x11Available = true;
    qDebug() << "X11 input simulation initialized (fallback)";

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
    }
}

void ActionExecutor::cleanupX11()
{
    if (m_display) {
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
}

void ActionExecutor::sendKeyX11(const QString &keys, Qt::KeyboardModifiers modifiers, int press)
{
    if (!m_display) {
        return;
    }

    if (press) {
        sendModifiersX11(modifiers, true);
        QThread::msleep(10);
    }

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

    if (!press) {
        QThread::msleep(10);
        sendModifiersX11(modifiers, false);
    }
}

void ActionExecutor::sendModifiersX11(Qt::KeyboardModifiers modifiers, int press)
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

    if (s_keySymMap.contains(lowerKey)) {
        return s_keySymMap[lowerKey];
    }

    // XStringToKeysym handles standard X11 keysym names
    KeySym sym = XStringToKeysym(key.toLatin1().data());
    if (sym != NoSymbol) {
        return sym;
    }

    return NoSymbol;
}
#endif
