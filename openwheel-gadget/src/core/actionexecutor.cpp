// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

// Qt headers MUST come before "actionexecutor.h" because that header
// pulls in X11/Xlib.h on X11 builds, and X11 #defines macros like `None`
// and `Always` which collide with Qt class members if included afterwards.
#include <QProcess>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QVariant>

#include "actionexecutor.h"
#include "desktopenvironment.h"

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
    m_isWayland = DesktopEnvironment::isWayland();
    qDebug() << "Session:" << DesktopEnvironment::sessionType()
             << "Desktop:" << DesktopEnvironment::currentDesktop()
             << (m_isWayland ? "(Wayland)" : "(X11)");
}

void ActionExecutor::executeAction(const ActionConfig &action, int repeatCount)
{
    for (int i = 0; i < repeatCount; ++i) {
        switch (action.type) {
            case ActionConfig::Type::Keyboard:
            case ActionConfig::Type::KeyboardRepeat:
                if (action.sticky) {
                    executeStickyKeyPress(action.keys, action.modifiers);
                } else {
                    executeKeyPress(action.keys, action.modifiers);
                }
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

    // Try native D-Bus / subprocess actions for well-known virtual keys first
    // (volume, brightness, zoom). These target system services and work on both
    // X11 and Wayland — no uinput or XTest needed for these paths.
    if (tryWaylandKeyAction(keys, modifiers)) {
        return;
    }

    // Next: ask the daemon to inject via uinput. This is the GNOME/Wayland
    // path for arbitrary keystrokes (Krita, Blender, switch-windows, ...).
    if (tryDaemonInjectKey(keys, modifiers)) {
        return;
    }

#ifdef HAVE_X11
    if (m_x11Available) {
        sendKeyX11(keys, modifiers, true);
        QThread::msleep(20);
        sendKeyX11(keys, modifiers, false);
        XFlush(m_display);
        return;
    }
#endif

    qWarning() << "Key simulation not available for:" << keys
               << "(daemon uinput injection unavailable and no X11 fallback)";
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

// ---------------------------------------------------------------------------
// Brightness — pluggable backends. Order: KDE first if running KDE (so
// PowerDevil's slider stays in sync), otherwise logind (universal Wayland
// backend), then GNOME settings daemon (older GNOME), then brightnessctl,
// then a final warning.
// ---------------------------------------------------------------------------

void ActionExecutor::brightnessChange(int direction)
{
    if (DesktopEnvironment::isKde()) {
        if (brightnessKdePowerDevil(direction)) return;
    }
    if (brightnessLogind(direction)) return;
    if (brightnessGnomeSettingsDaemon(direction)) return;
    if (brightnessctlSubprocess(direction)) return;

    qWarning() << "brightness: no working backend (tried logind, GNOME SD, brightnessctl, PowerDevil)";
}

QString ActionExecutor::discoverBacklightDevice()
{
    if (m_backlightProbed) return m_backlightDevice;
    m_backlightProbed = true;

    QDir d(QStringLiteral("/sys/class/backlight"));
    const QStringList entries = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) {
        m_backlightDevice = QString();
        qDebug() << "brightness: no backlight devices in /sys/class/backlight";
        return m_backlightDevice;
    }
    // Prefer non-firmware/intel/amdgpu devices in that order; otherwise pick first.
    for (const QString &name : entries) {
        if (name.contains(QStringLiteral("intel"), Qt::CaseInsensitive) ||
            name.contains(QStringLiteral("amdgpu"), Qt::CaseInsensitive)) {
            m_backlightDevice = name;
            qDebug() << "brightness: using backlight" << m_backlightDevice;
            return m_backlightDevice;
        }
    }
    m_backlightDevice = entries.first();
    qDebug() << "brightness: using backlight" << m_backlightDevice;
    return m_backlightDevice;
}

bool ActionExecutor::readBacklightValues(const QString &device, int *current, int *max)
{
    if (device.isEmpty() || !current || !max) return false;
    const QString base = QStringLiteral("/sys/class/backlight/") + device + QStringLiteral("/");

    QFile cur(base + QStringLiteral("brightness"));
    QFile mx (base + QStringLiteral("max_brightness"));
    if (!cur.open(QIODevice::ReadOnly) || !mx.open(QIODevice::ReadOnly)) {
        return false;
    }
    bool ok1 = false, ok2 = false;
    *current = QString::fromLatin1(cur.readAll()).trimmed().toInt(&ok1);
    *max     = QString::fromLatin1(mx.readAll()).trimmed().toInt(&ok2);
    return ok1 && ok2 && *max > 0;
}

bool ActionExecutor::brightnessLogind(int direction)
{
    const QString device = discoverBacklightDevice();
    if (device.isEmpty()) return false;

    int current = 0, max = 0;
    if (!readBacklightValues(device, &current, &max)) {
        qDebug() << "brightness: failed to read sysfs for" << device;
        return false;
    }

    const int step = qMax(1, max / 20);  // 5% steps
    const int newValue = qBound(0, current + (direction * step), max);

    // logind exposes SetBrightness on the user's session. We call the active
    // session via the convenience path "/org/freedesktop/login1/session/auto".
    auto bus = QDBusConnection::systemBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1/session/auto"),
        QStringLiteral("org.freedesktop.login1.Session"),
        QStringLiteral("SetBrightness"));
    msg << QStringLiteral("backlight") << device << static_cast<quint32>(newValue);

    QDBusMessage reply = bus.call(msg, QDBus::Block, 500);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qDebug() << "brightness: logind SetBrightness failed:" << reply.errorMessage();
        return false;
    }

    qreal pct = (static_cast<qreal>(newValue) / max) * 100.0;
    Q_EMIT systemValueChanged(pct, 0.0, 100.0);
    qDebug() << "brightness (logind):" << current << "->" << newValue << "/" << max;
    return true;
}

bool ActionExecutor::brightnessGnomeSettingsDaemon(int direction)
{
    auto bus = QDBusConnection::sessionBus();
    const QString service = QStringLiteral("org.gnome.SettingsDaemon.Power");
    const QString path    = QStringLiteral("/org/gnome/SettingsDaemon/Power");
    const QString iface   = QStringLiteral("org.gnome.SettingsDaemon.Power.Screen");

    // Get current value via Properties.Get
    QDBusMessage getMsg = QDBusMessage::createMethodCall(
        service, path, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    getMsg << iface << QStringLiteral("Brightness");
    QDBusMessage getReply = bus.call(getMsg, QDBus::Block, 500);
    if (getReply.type() == QDBusMessage::ErrorMessage) {
        qDebug() << "brightness: GNOME SD Get failed:" << getReply.errorMessage();
        return false;
    }
    if (getReply.arguments().isEmpty()) return false;

    int current = getReply.arguments().first().value<QDBusVariant>().variant().toInt();
    int newValue = qBound(0, current + (direction * 5), 100);  // GNOME uses percent

    QDBusMessage setMsg = QDBusMessage::createMethodCall(
        service, path, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Set"));
    setMsg << iface << QStringLiteral("Brightness")
           << QVariant::fromValue(QDBusVariant(QVariant::fromValue(newValue)));
    QDBusMessage setReply = bus.call(setMsg, QDBus::Block, 500);
    if (setReply.type() == QDBusMessage::ErrorMessage) {
        qDebug() << "brightness: GNOME SD Set failed:" << setReply.errorMessage();
        return false;
    }

    Q_EMIT systemValueChanged(newValue, 0.0, 100.0);
    qDebug() << "brightness (GNOME SD):" << current << "->" << newValue << "%";
    return true;
}

bool ActionExecutor::brightnessKdePowerDevil(int direction)
{
    auto bus = QDBusConnection::sessionBus();
    const QString service = QStringLiteral("org.kde.Solid.PowerManagement");
    const QString path    = QStringLiteral("/org/kde/Solid/PowerManagement/Actions/BrightnessControl");
    const QString iface   = QStringLiteral("org.kde.Solid.PowerManagement.Actions.BrightnessControl");

    QDBusReply<int> currentReply = bus.call(
        QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("brightness")));
    QDBusReply<int> maxReply = bus.call(
        QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("brightnessMax")));

    if (!currentReply.isValid() || !maxReply.isValid()) {
        qDebug() << "brightness: PowerDevil unavailable";
        return false;
    }

    int current = currentReply.value();
    int max = maxReply.value();
    int step = qMax(1, max / 20);
    int newBrightness = qBound(0, current + (direction * step), max);

    QDBusMessage setMsg = QDBusMessage::createMethodCall(service, path, iface,
        QStringLiteral("setBrightness"));
    setMsg << newBrightness;
    bus.call(setMsg, QDBus::NoBlock);

    qreal pct = (max > 0) ? (static_cast<qreal>(newBrightness) / max * 100.0) : 0.0;
    Q_EMIT systemValueChanged(pct, 0.0, 100.0);
    qDebug() << "brightness (PowerDevil):" << current << "->" << newBrightness << "/" << max;
    return true;
}

bool ActionExecutor::brightnessctlSubprocess(int direction)
{
    // brightnessctl is a small CLI that pokes /sys/class/backlight directly.
    // Optional final fallback for systems without logind brightness support.
    QFileInfo fi(QStringLiteral("/usr/bin/brightnessctl"));
    if (!fi.exists()) {
        fi.setFile(QStringLiteral("/usr/local/bin/brightnessctl"));
        if (!fi.exists()) return false;
    }

    QStringList args;
    args << QStringLiteral("set") << (direction > 0 ? QStringLiteral("5%+") : QStringLiteral("5%-"));
    QProcess proc;
    proc.start(fi.absoluteFilePath(), args);
    if (!proc.waitForFinished(500) || proc.exitCode() != 0) {
        qDebug() << "brightness: brightnessctl failed:" << proc.readAllStandardError();
        return false;
    }

    // Read back current/max for the overlay value.
    int current = 0, max = 0;
    const QString device = discoverBacklightDevice();
    if (!device.isEmpty() && readBacklightValues(device, &current, &max)) {
        qreal pct = (static_cast<qreal>(current) / max) * 100.0;
        Q_EMIT systemValueChanged(pct, 0.0, 100.0);
    }
    qDebug() << "brightness (brightnessctl):" << (direction > 0 ? "+5%" : "-5%");
    return true;
}

// ---------------------------------------------------------------------------
// Zoom — KWin's compositor zoom only exists on KDE. Everywhere else fall back
// to a regular Ctrl+= / Ctrl+- keystroke pair (which most apps interpret as
// "zoom in / out": browsers, image viewers, IDEs, etc.). The keystroke is
// routed through the daemon's uinput injector by way of executeKeyPress().
// ---------------------------------------------------------------------------

void ActionExecutor::zoomChange(int direction)
{
    if (DesktopEnvironment::isKde()) {
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
        qDebug() << "zoom (KWin):" << shortcut;
        return;
    }

    // Application zoom: Ctrl + = / Ctrl + -
    // Signal the overlay to lower itself so the compositor gives keyboard focus
    // back to the user's app before we inject the keystroke.
    Q_EMIT focusEscapeRequired();
    QThread::msleep(60);   // give compositor time to process the focus change
    const QString key = (direction > 0) ? QStringLiteral("equal") : QStringLiteral("minus");
    executeKeyPress(key, Qt::ControlModifier);
    qDebug() << "zoom (keystroke): Ctrl+" << (direction > 0 ? "=" : "-");
}

void ActionExecutor::executeMouseScroll(int delta, Qt::Orientation orientation)
{
    // Prefer the daemon's uinput injection — works on both X11 and Wayland.
    if (tryDaemonInjectScroll(delta, orientation)) {
        return;
    }

#ifdef HAVE_X11
    if (m_x11Available) {
        sendMouseScrollX11(delta, orientation);
        XFlush(m_display);
        return;
    }
#endif
    qWarning() << "Mouse scroll simulation not available (daemon uinput unavailable and no X11 fallback)";
}

// ---------------------------------------------------------------------------
// Daemon-backed input injection (org.asus.dial.InjectKey / InjectScroll).
// The daemon exposes a virtual uinput device, so this path works under any
// Wayland compositor without ydotool or compositor-specific protocols.
// ---------------------------------------------------------------------------

bool ActionExecutor::daemonInjectionAvailable()
{
    if (m_daemonInjectAvailable != -1) {
        return m_daemonInjectAvailable == 1;
    }

    auto bus = QDBusConnection::sessionBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.asus.dial"),
        QStringLiteral("/org/asus/dial"),
        QStringLiteral("org.asus.dial"),
        QStringLiteral("InjectionAvailable"));
    QDBusReply<bool> reply = bus.call(msg, QDBus::Block, 200);
    if (reply.isValid()) {
        if (reply.value()) {
            m_daemonInjectAvailable = 1;
        }
        // Don't cache false — uinput may become available after daemon restarts
        qDebug() << "Daemon injection available:" << (m_daemonInjectAvailable == 1);
    } else {
        // Don't cache a negative — the daemon might be starting up.
        // Re-probe the next time we need it.
        qDebug() << "Daemon injection probe failed:" << reply.error().message();
        return false;
    }
    return m_daemonInjectAvailable == 1;
}

bool ActionExecutor::tryDaemonInjectKey(const QString &keys, Qt::KeyboardModifiers modifiers)
{
    if (!daemonInjectionAvailable()) {
        return false;
    }

    // Translate Qt::KeyboardModifiers to the daemon's OW_MOD_* bitmask.
    quint32 mods = 0;
    if (modifiers & Qt::ControlModifier) mods |= 0x01;
    if (modifiers & Qt::ShiftModifier)   mods |= 0x02;
    if (modifiers & Qt::AltModifier)     mods |= 0x04;
    if (modifiers & Qt::MetaModifier)    mods |= 0x08;

    auto bus = QDBusConnection::sessionBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.asus.dial"),
        QStringLiteral("/org/asus/dial"),
        QStringLiteral("org.asus.dial"),
        QStringLiteral("InjectKey"));
    msg << keys << mods;

    QDBusReply<bool> reply = bus.call(msg, QDBus::Block, 200);
    if (!reply.isValid()) {
        qWarning() << "InjectKey failed:" << reply.error().message();
        // Daemon may have died — invalidate cache so we reprobe.
        m_daemonInjectAvailable = -1;
        return false;
    }
    if (!reply.value()) {
        // Daemon doesn't recognise this key — fall through to other backends.
        qDebug() << "InjectKey returned false for:" << keys;
        return false;
    }
    return true;
}

bool ActionExecutor::tryDaemonInjectScroll(int delta, Qt::Orientation orientation)
{
    if (!daemonInjectionAvailable()) {
        return false;
    }
    if (delta == 0) {
        return true;
    }

    auto bus = QDBusConnection::sessionBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.asus.dial"),
        QStringLiteral("/org/asus/dial"),
        QStringLiteral("org.asus.dial"),
        QStringLiteral("InjectScroll"));
    msg << static_cast<qint32>(delta) << (orientation == Qt::Horizontal);

    QDBusReply<bool> reply = bus.call(msg, QDBus::Block, 200);
    if (!reply.isValid()) {
        qWarning() << "InjectScroll failed:" << reply.error().message();
        m_daemonInjectAvailable = -1;
        return false;
    }
    return reply.value();
}

// ---------------------------------------------------------------------------
// Sticky modifier support — keeps modifier keys held across multiple rotation
// ticks so that e.g. Alt stays down during an Alt+Tab window-switch sequence.
// releaseStickyModifiers() must be called when the dial button is released.
// ---------------------------------------------------------------------------

// Translate Qt modifier flags to the OW_MOD_* bitmask used by the daemon.
static quint32 qtModsToOwMods(Qt::KeyboardModifiers modifiers)
{
    quint32 mods = 0;
    if (modifiers & Qt::ControlModifier) mods |= 0x01u; // OW_MOD_CTRL
    if (modifiers & Qt::ShiftModifier)   mods |= 0x02u; // OW_MOD_SHIFT
    if (modifiers & Qt::AltModifier)     mods |= 0x04u; // OW_MOD_ALT
    if (modifiers & Qt::MetaModifier)    mods |= 0x08u; // OW_MOD_SUPER
    return mods;
}

bool ActionExecutor::tryDaemonHoldModifiers(quint32 owMods)
{
    if (!daemonInjectionAvailable()) return false;
    auto bus = QDBusConnection::sessionBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.asus.dial"),
        QStringLiteral("/org/asus/dial"),
        QStringLiteral("org.asus.dial"),
        QStringLiteral("HoldModifiers"));
    msg << owMods;
    QDBusReply<bool> reply = bus.call(msg, QDBus::Block, 200);
    if (!reply.isValid()) {
        qWarning() << "HoldModifiers D-Bus call failed:" << reply.error().message();
        m_daemonInjectAvailable = -1;
        return false;
    }
    return reply.value();
}

bool ActionExecutor::tryDaemonReleaseModifiers(quint32 owMods)
{
    if (!daemonInjectionAvailable()) return false;
    auto bus = QDBusConnection::sessionBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.asus.dial"),
        QStringLiteral("/org/asus/dial"),
        QStringLiteral("org.asus.dial"),
        QStringLiteral("ReleaseModifiers"));
    msg << owMods;
    QDBusReply<bool> reply = bus.call(msg, QDBus::Block, 200);
    if (!reply.isValid()) {
        qWarning() << "ReleaseModifiers D-Bus call failed:" << reply.error().message();
        m_daemonInjectAvailable = -1;
        return false;
    }
    return reply.value();
}

void ActionExecutor::executeStickyKeyPress(const QString &keys, Qt::KeyboardModifiers modifiers)
{
    const quint32 newOwMods  = qtModsToOwMods(modifiers);
    const quint32 curOwMods  = qtModsToOwMods(m_stickyModsHeld);

    // Determine which modifiers need to be added or removed incrementally.
    // We avoid releasing and re-pressing shared modifiers so that e.g. the
    // window-switcher overlay doesn't flash closed when switching direction.
    const quint32 toAdd    = newOwMods & ~curOwMods;
    const quint32 toRemove = curOwMods & ~newOwMods;

    if (toAdd)    tryDaemonHoldModifiers(toAdd);
    if (toRemove) tryDaemonReleaseModifiers(toRemove);

    m_stickyModsHeld = modifiers;
    m_stickyActive   = (modifiers != Qt::NoModifier);

    // Tap the key — modifiers are already physically held so pass 0 to avoid
    // double-pressing them (InjectKey with mods=0 just taps the key alone).
    tryDaemonInjectKey(keys, Qt::NoModifier);
    qDebug() << "StickyKey tap:" << keys << "held-mods:" << modifiers;
}

void ActionExecutor::releaseStickyModifiers()
{
    if (!m_stickyActive) return;
    const quint32 owMods = qtModsToOwMods(m_stickyModsHeld);
    if (owMods) tryDaemonReleaseModifiers(owMods);
    m_stickyModsHeld = Qt::NoModifier;
    m_stickyActive   = false;
    qDebug() << "StickyKey: modifiers released";
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
        // Read directly from /sys/class/backlight — works on any Linux
        // regardless of desktop environment.
        const QString device = discoverBacklightDevice();
        int current = 0, max = 0;
        if (!device.isEmpty() && readBacklightValues(device, &current, &max)) {
            qreal pct = static_cast<qreal>(current) / max * 100.0;
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
