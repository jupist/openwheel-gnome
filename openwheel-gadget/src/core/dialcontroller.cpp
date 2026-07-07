// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include "dialcontroller.h"
#include "rotationhandler.h"
#include "actionexecutor.h"
#include "../profiles/profilemanager.h"
#include "../profiles/applicationmatcher.h"
#include "../dbus/dbusinterface.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QSet>
#include <QSettings>
#include <QProcess>

DialController::DialController(QObject *parent)
    : QObject(parent)
    , m_dbusInterface(new DBusInterface())
    , m_profileManager(new ProfileManager())
    , m_appMatcher(new ApplicationMatcher())
    , m_actionExecutor(new ActionExecutor())
    , m_rotationHandler(new RotationHandler())
{
    setupConnections();
}

DialController::~DialController() = default;

void DialController::initialize()
{
    qDebug() << "Initializing DialController";

    // Load all profiles
    m_profileManager->loadProfiles();

    // Restore the previously selected profile (or fall back to default).
    const QString saved = loadPersistedProfile();
    if (!saved.isEmpty() && !m_profileManager->getProfile(saved) == false) {
        switchToProfile(saved);
    }

    // Restore press-to-activate preference.
    QSettings prefs(QStringLiteral("openwheel"), QStringLiteral("gadget"));
    m_pressToActivate = prefs.value(QStringLiteral("pressToActivate"), true).toBool();

    // Start monitoring (no-op on GNOME; signals only fire via manual picker).
    m_appMatcher->startMonitoring();

    qDebug() << "DialController initialized, profile:" << currentProfileName();
}

void DialController::setupConnections()
{
    // D-Bus interface connections
    connect(m_dbusInterface.get(), &DBusInterface::rotationChanged,
            this, &DialController::onRotationChanged);
    connect(m_dbusInterface.get(), &DBusInterface::buttonPressed,
            this, &DialController::onButtonPressed);
    connect(m_dbusInterface.get(), &DBusInterface::buttonReleased,
            this, &DialController::onButtonReleased);

    // Application matcher connections
    connect(m_appMatcher.get(), &ApplicationMatcher::windowChanged,
            this, &DialController::onWindowChanged);
    connect(m_appMatcher.get(), &ApplicationMatcher::profileIdRequested,
            this, &DialController::onProfileIdRequested);

    // Profile manager connections
    connect(m_profileManager.get(), &ProfileManager::currentProfileChanged,
            this, [this]() {
                Q_EMIT profileChanged(currentProfileName());
                updateCurrentFunction();
                Q_EMIT functionsChanged();
            });
    connect(m_profileManager.get(), &ProfileManager::profilesChanged,
            this, [this]() {
                Q_EMIT profilesChanged();
            });

    // Rotation handler connections
    connect(m_rotationHandler.get(), &RotationHandler::actionTriggered,
            this, &DialController::onActionTriggered);
    // Only forward rotationTick to QML when the overlay should be shown.
    // Scroll and zoom functions suppress the overlay entirely.
    connect(m_rotationHandler.get(), &RotationHandler::rotationTick,
            this, [this]() {
                if (!m_currentFunctionNeedsEscape)
                    Q_EMIT rotationTick();
            });
    // RotationHandler::valueChanged carries raw accumulated degrees — don't
    // forward it to QML.  Real system values come from ActionExecutor instead.
    connect(m_rotationHandler.get(), &RotationHandler::adjustingChanged,
            this, [this](int adjusting) {
                if (!m_currentFunctionNeedsEscape)
                    Q_EMIT adjustingChanged(adjusting);
            });

    // When the action executor reports a real system value, update the UI
    connect(m_actionExecutor.get(), &ActionExecutor::systemValueChanged,
            this, [this](qreal value, qreal, qreal) {
                Q_EMIT valueChanged(value);
            });

    // When MPRIS reports new track info, update the exposed properties
    connect(m_actionExecutor.get(), &ActionExecutor::mediaInfoChanged,
            this, [this](const QString &title, const QString &artist) {
                m_mediaTitle  = title;
                m_mediaArtist = artist;
                Q_EMIT mediaTitleChanged();
            });
}

void DialController::activate()
{
    if (m_active) {
        return;
    }

    m_active = true;
    Q_EMIT activeChanged(true);

    qDebug() << "Dial activated";
}

void DialController::deactivate()
{
    if (!m_active) {
        return;
    }

    m_active = false;
    m_rotationHandler->reset();
    Q_EMIT activeChanged(false);

    qDebug() << "Dial deactivated";
}

void DialController::toggle()
{
    if (m_active) {
        deactivate();
    } else {
        activate();
    }
}

void DialController::selectNext()
{
    const Profile *profile = m_profileManager->getCurrentProfile();
    if (!profile) {
        return;
    }

    int functionCount = profile->getMenuFunctions().size();
    if (functionCount == 0) {
        return;
    }

    m_selectedIndex = (m_selectedIndex + 1) % functionCount;
    Q_EMIT selectionChanged(m_selectedIndex);

    updateCurrentFunction();

    qDebug() << "Selected function index:" << m_selectedIndex;
}

void DialController::selectPrevious()
{
    const Profile *profile = m_profileManager->getCurrentProfile();
    if (!profile) {
        return;
    }

    int functionCount = profile->getMenuFunctions().size();
    if (functionCount == 0) {
        return;
    }

    m_selectedIndex = (m_selectedIndex - 1 + functionCount) % functionCount;
    Q_EMIT selectionChanged(m_selectedIndex);

    updateCurrentFunction();

    qDebug() << "Selected function index:" << m_selectedIndex;
}

void DialController::confirmSelection()
{
    qDebug() << "Function confirmed";
}

// ── Profile picker ────────────────────────────────────────────────────────

// Returns enabled-profile IDs in load order — the exact same sequence that
// availableProfiles() uses for the picker QML model (excluding __settings__).
// All picker index arithmetic must use this list so QML and C++ agree.
QStringList DialController::enabledProfileIds() const
{
    QStringList result;
    const QStringList all = m_profileManager->getProfileIds();
    for (const QString &id : all) {
        const Profile *p = m_profileManager->getProfile(id);
        if (p && p->enabled) result.append(id);
    }
    return result;
}

QVariantList DialController::availableProfiles() const
{
    QVariantList result;
    for (const QString &id : enabledProfileIds()) {
        const Profile *p = m_profileManager->getProfile(id);
        QVariantMap m;
        m[QStringLiteral("id")]   = id;
        m[QStringLiteral("name")] = p->displayName;
        result.append(m);
    }
    // Always append a sentinel entry so the picker gives access to Settings
    // even when no system tray is available (common on stock GNOME).
    QVariantMap settingsEntry;
    settingsEntry[QStringLiteral("id")]   = QStringLiteral("__settings__");
    settingsEntry[QStringLiteral("name")] = QStringLiteral("⚙  Settings");
    result.append(settingsEntry);
    return result;
}

void DialController::openProfilePicker()
{
    if (m_pickerActive) return;

    // Seed pickerIndex at the current profile — use enabled-only list so the
    // index matches what the QML picker model shows.
    const QStringList ids = enabledProfileIds();
    int idx = ids.indexOf(m_currentProfileId);
    m_pickerIndex = (idx >= 0) ? idx : 0;

    m_pickerActive = 1;
    Q_EMIT pickerActiveChanged(1);
    Q_EMIT pickerIndexChanged(m_pickerIndex);
    qDebug() << "Profile picker opened";
}

void DialController::openFunctionPicker()
{
    if (m_pickerActive) return;

    const QVariantList funcs = currentFunctions();
    if (funcs.isEmpty()) return;

    m_pickerIndex = (m_selectedIndex >= 0 && m_selectedIndex < funcs.size()) ? m_selectedIndex : 0;
    m_pickerTickAccum = 0;
    m_pickerActive = 2;
    Q_EMIT pickerActiveChanged(2);
    Q_EMIT pickerIndexChanged(m_pickerIndex);
    qDebug() << "Function picker opened";
}

void DialController::closeProfilePicker()
{
    if (!m_pickerActive) return;
    m_pickerActive = 0;
    Q_EMIT pickerActiveChanged(0);
    qDebug() << "Profile picker closed (cancelled)";
}

void DialController::confirmProfilePicker()
{
    if (!m_pickerActive) return;

    if (m_pickerActive == 2) {
        // Function picker mode
        const QVariantList funcs = currentFunctions();
        if (m_pickerIndex >= 0 && m_pickerIndex < funcs.size()) {
            if (m_selectedIndex != m_pickerIndex) {
                m_selectedIndex = m_pickerIndex;
                Q_EMIT selectionChanged(m_selectedIndex);
                updateCurrentFunction();
            }
        }
        m_pickerActive = 0;
        Q_EMIT pickerActiveChanged(0);
        qDebug() << "Function picker confirmed index:" << m_selectedIndex;
        return;
    }

    // Profile picker mode (m_pickerActive == 1)
    // Use the same enabled-only list the QML picker model was built from.
    const QStringList ids = enabledProfileIds();
    if (m_pickerIndex >= 0 && m_pickerIndex < ids.size()) {
        // Normal profile entry
        setActiveProfile(ids[m_pickerIndex]);
    } else if (m_pickerIndex == ids.size()) {
        // Settings sentinel is always the last entry (ids.size() index)
        m_pickerActive = 0;
        Q_EMIT pickerActiveChanged(0);
        Q_EMIT settingsRequested();
        return;
    }
    m_pickerActive = 0;
    Q_EMIT pickerActiveChanged(0);
}

void DialController::setActiveProfile(const QString &profileId)
{
    if (profileId.isEmpty()) return;
    if (!m_profileManager->getProfile(profileId)) {
        qWarning() << "setActiveProfile: unknown profile" << profileId;
        return;
    }
    switchToProfile(profileId);
    persistSelectedProfile(profileId);
    qDebug() << "Profile manually set to:" << profileId;
}

// ── QSettings persistence ─────────────────────────────────────────────────

void DialController::persistSelectedProfile(const QString &profileId)
{
    QSettings s(QStringLiteral("openwheel"), QStringLiteral("gadget"));
    s.setValue(QStringLiteral("selectedProfile"), profileId);
}

QString DialController::loadPersistedProfile() const
{
    QSettings s(QStringLiteral("openwheel"), QStringLiteral("gadget"));
    return s.value(QStringLiteral("selectedProfile")).toString();
}

void DialController::onRotationChanged(int delta)
{
    qDebug() << "DialController: Rotation changed:" << delta;

    // Profile or function picker steals rotation when open — 2 ticks per step to prevent
    // accidental selection when the user's hand is unsteady.
    if (m_pickerActive) {
        int total = 0;
        if (m_pickerActive == 1) {
            total = enabledProfileIds().size() + 1; // enabled profiles + settings sentinel
        } else if (m_pickerActive == 2) {
            total = currentFunctions().size();
        }
        if (total <= 0) return;
        m_pickerTickAccum += delta;
        while (m_pickerTickAccum >= 2) {
            m_pickerIndex = (m_pickerIndex + 1 + total) % total;
            Q_EMIT pickerIndexChanged(m_pickerIndex);
            m_pickerTickAccum -= 2;
        }
        while (m_pickerTickAccum <= -2) {
            m_pickerIndex = (m_pickerIndex - 1 + total) % total;
            Q_EMIT pickerIndexChanged(m_pickerIndex);
            m_pickerTickAccum += 2;
        }
        return;
    }

    // Press-to-activate: ignore rotation unless button is held (if mode is on).
    if (m_pressToActivate && !m_buttonHeld) return;

    // Rotation while held → cancel long-press timer (not a "hold for picker" gesture)
    if (m_longPressTimer && m_longPressTimer->isActive()) {
        m_longPressTimer->stop();
    }
    m_rotatedDuringHold = true;

    // Pass to the rotation/action handler — it will emit rotationTick and
    // show the overlay via the normal QML Connections handler.
    m_rotationHandler->onRotation(delta);
}

void DialController::onButtonPressed()
{
    qDebug() << "DialController: Button pressed";

    m_buttonHeld = true;
    m_rotatedDuringHold = false;
    m_longPressTriggered = false;

    // If we are in the middle of a multi-click sequence (e.g. second press of double-click),
    // stop the decision timer so it doesn't expire while the button is pressed down.
    if (m_clickDecisionTimer && m_clickDecisionTimer->isActive()) {
        m_clickDecisionTimer->stop();
    }

    // Only start long-press timer if this is the initial press (not part of a multi-click).
    if (m_clickCount == 0) {
        if (!m_longPressTimer) {
            m_longPressTimer = new QTimer(this);
            m_longPressTimer->setSingleShot(true);
            m_longPressTimer->setInterval(LONG_PRESS_MS);
            connect(m_longPressTimer, &QTimer::timeout, this, [this]() {
                m_longPressTriggered = true;
                m_clickCount = 0; // swallow pending click
                if (m_pickerActive) {
                    confirmProfilePicker();
                } else {
                    m_pickerTickAccum = 0;
                    openProfilePicker();
                }
            });
        }
        m_longPressTimer->start();
    }
}

void DialController::onButtonReleased()
{
    qDebug() << "DialController: Button released";

    m_buttonHeld = false;

    // Release any sticky modifier keys held during rotation (window switcher etc.)
    m_actionExecutor->releaseStickyModifiers();

    if (m_longPressTimer) m_longPressTimer->stop();

    if (m_longPressTriggered) {
        m_longPressTriggered = false;
        // Already handled by the timer callback — nothing more to do.
        return;
    }

    // If picker is open, a short press confirms the selection.
    if (m_pickerActive) {
        confirmProfilePicker();
        return;
    }

    // If the user rotated during this hold (press-to-activate mode), the
    // rotation was the action — don't also count it as a button click.
    if (m_pressToActivate && m_rotatedDuringHold) {
        m_rotatedDuringHold = false;
        return;
    }
    m_rotatedDuringHold = false;

    // Short press, no rotation: single or double click.
    m_clickCount++;

    if (m_clickCount == 1) {
        if (!m_clickDecisionTimer) {
            m_clickDecisionTimer = new QTimer(this);
            m_clickDecisionTimer->setSingleShot(true);
            m_clickDecisionTimer->setInterval(DOUBLE_CLICK_INTERVAL);
            connect(m_clickDecisionTimer, &QTimer::timeout, this, [this]() {
                if (m_clickCount == 1) {
                    executeClickAction();
                }
                m_clickCount = 0;
            });
        }
        m_clickDecisionTimer->start();
    } else if (m_clickCount >= 2) {
        m_clickDecisionTimer->stop();
        m_clickCount = 0;
        openFunctionPicker();
        qDebug() << "Double click -> open function picker";
    }
}

void DialController::executeClickAction()
{
    const Profile *profile = m_profileManager->getCurrentProfile();
    if (!profile) return;

    QVector<Function> funcs = profile->getMenuFunctions();
    if (m_selectedIndex < 0 || m_selectedIndex >= funcs.size()) return;

    const ActionConfig &click = funcs[m_selectedIndex].clickAction;

    // Skip empty keyboard actions (no click action defined in the profile).
    if ((click.type == ActionConfig::Type::Keyboard ||
         click.type == ActionConfig::Type::KeyboardRepeat) &&
        click.keys.isEmpty()) {
        qDebug() << "Single click: no click action defined for this function";
        return;
    }

    qDebug() << "Single click -> executing click action";
    m_actionExecutor->executeAction(click, 1);
}

void DialController::onWindowChanged(const QString &windowClass,
                                      const QString &windowName,
                                      const QString &processName)
{
    qDebug() << "DialController: Window changed -" << windowClass << "/" << processName;

    // Find matching profile via pattern matching (KDE auto-detection only).
    QString profileId = m_profileManager->findMatchingProfile(windowClass, windowName, processName);

    if (!profileId.isEmpty() && profileId != m_currentProfileId) {
        switchToProfile(profileId);
    }
}

void DialController::onProfileIdRequested(const QString &profileId)
{
    // Direct profile switch from ApplicationMatcher::setCurrentProfileById().
    setActiveProfile(profileId);
}

void DialController::onActionTriggered(const ActionConfig &action, int repeatCount)
{
    qDebug() << "DialController: Executing action" << repeatCount << "times";

    m_actionExecutor->executeAction(action, repeatCount);

    // After a media key (skip/seek/play), refresh the track info display.
    // queryMediaInfo runs in a background thread so it never blocks the
    // event loop (which would break double-click / long-press timers).
    static const QSet<QString> kMediaKeys = {
        QStringLiteral("XF86AudioNext"),
        QStringLiteral("XF86AudioPrev"),
        QStringLiteral("XF86AudioPlay"),
        QStringLiteral("XF86AudioPause"),
        QStringLiteral("XF86AudioStop"),
    };
    if (kMediaKeys.contains(action.keys) || action.command.startsWith(QStringLiteral("playerctl"))) {
        m_actionExecutor->queryMediaInfo(600);
    }
}

void DialController::switchToProfile(const QString &profileId)
{
    qDebug() << "Switching to profile:" << profileId;

    m_currentProfileId = profileId;
    m_profileManager->setCurrentProfile(profileId);

    // Reset selection
    m_selectedIndex = 0;
    Q_EMIT selectionChanged(m_selectedIndex);

    // Update current function
    updateCurrentFunction();

    Q_EMIT profileChanged(currentProfileName());
    Q_EMIT functionsChanged();
}

void DialController::updateCurrentFunction()
{
    const Profile *profile = m_profileManager->getCurrentProfile();
    if (!profile) {
        qWarning() << "No current profile";
        return;
    }

    QVector<Function> menuFunctions = profile->getMenuFunctions();
    if (menuFunctions.isEmpty()) {
        qWarning() << "Profile has no menu functions";
        return;
    }

    if (m_selectedIndex < 0 || m_selectedIndex >= menuFunctions.size()) {
        m_selectedIndex = 0;
    }

    const Function &func = menuFunctions[m_selectedIndex];

    qDebug() << "Current function:" << func.label;

    // Update rotation handler with new actions
    m_rotationHandler->setCurrentActions(func.clockwiseAction, func.counterClockwiseAction);
    // Query real system value for the new function
    m_actionExecutor->queryCurrentValue(func.clockwiseAction.keys);
    m_rotationHandler->reset();

    // Update the escape flag so QML can reliably suppress the overlay.
    static const QStringList kFocusEscapeKeys = {
        QStringLiteral("ZoomIn"), QStringLiteral("ZoomOut")
    };
    const bool needsEscape =
        func.suppressOverlay ||
        func.clockwiseAction.type        == ActionConfig::Type::MouseScroll ||
        func.counterClockwiseAction.type == ActionConfig::Type::MouseScroll ||
        func.clockwiseAction.sticky      || func.counterClockwiseAction.sticky ||
        kFocusEscapeKeys.contains(func.clockwiseAction.keys) ||
        kFocusEscapeKeys.contains(func.counterClockwiseAction.keys);
    if (needsEscape != m_currentFunctionNeedsEscape) {
        m_currentFunctionNeedsEscape = needsEscape;
        Q_EMIT currentFunctionNeedsEscapeChanged();
    }

    Q_EMIT functionChanged();
}

QString DialController::currentProfileName() const
{
    return m_profileManager->currentProfileName();
}

QVariantList DialController::currentFunctions() const
{
    QVariantList result;

    const Profile *profile = m_profileManager->getCurrentProfile();
    if (!profile) {
        return result;
    }

    QVector<Function> functions = profile->getMenuFunctions();
    for (const Function &func : functions) {
        result.append(functionToVariantMap(func));
    }

    return result;
}

int DialController::isAdjusting() const
{
    return m_rotationHandler->isAdjusting();
}

qreal DialController::currentValue() const
{
    return m_rotationHandler->currentValue();
}

QString DialController::currentUnit() const
{
    const Profile *profile = m_profileManager->getCurrentProfile();
    if (!profile) {
        return QString();
    }

    QVector<Function> functions = profile->getMenuFunctions();
    if (m_selectedIndex >= 0 && m_selectedIndex < functions.size()) {
        return functions[m_selectedIndex].unit;
    }

    return QString();
}

qreal DialController::currentMinValue() const
{
    const Profile *profile = m_profileManager->getCurrentProfile();
    if (!profile) {
        return 0.0;
    }

    QVector<Function> functions = profile->getMenuFunctions();
    if (m_selectedIndex >= 0 && m_selectedIndex < functions.size()) {
        return functions[m_selectedIndex].minValue.value_or(0.0);
    }

    return 0.0;
}

qreal DialController::currentMaxValue() const
{
    const Profile *profile = m_profileManager->getCurrentProfile();
    if (!profile) {
        return 100.0;
    }

    QVector<Function> functions = profile->getMenuFunctions();
    if (m_selectedIndex >= 0 && m_selectedIndex < functions.size()) {
        return functions[m_selectedIndex].maxValue.value_or(100.0);
    }

    return 100.0;
}

void DialController::setPressToActivate(int enabled)
{
    const bool b = (enabled != 0);
    if (m_pressToActivate == b) return;
    m_pressToActivate = b;
    QSettings prefs(QStringLiteral("openwheel"), QStringLiteral("gadget"));
    prefs.setValue(QStringLiteral("pressToActivate"), b);
    Q_EMIT pressToActivateChanged();
    qDebug() << "Press-to-activate:" << (b ? "on" : "off");
}

int DialController::daemonAutostart() const
{
    QProcess process;
    process.start(QStringLiteral("systemctl"), QStringList() << QStringLiteral("--user") << QStringLiteral("is-enabled") << QStringLiteral("openwheel-daemon"));
    process.waitForFinished(500);
    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    return (output == QStringLiteral("enabled")) ? 1 : 0;
}

void DialController::setDaemonAutostart(int enabled)
{
    const bool b = (enabled != 0);
    if (daemonAutostart() == (b ? 1 : 0)) return;

    QString cmd = b ? QStringLiteral("enable") : QStringLiteral("disable");
    QProcess::execute(QStringLiteral("systemctl"), QStringList() << QStringLiteral("--user") << cmd << QStringLiteral("openwheel-daemon"));

    if (b) {
        QProcess::startDetached(QStringLiteral("systemctl"), QStringList() << QStringLiteral("--user") << QStringLiteral("start") << QStringLiteral("openwheel-daemon"));
    } else {
        QProcess::startDetached(QStringLiteral("systemctl"), QStringList() << QStringLiteral("--user") << QStringLiteral("stop") << QStringLiteral("openwheel-daemon"));
    }

    Q_EMIT daemonAutostartChanged();
    qDebug() << "Daemon autostart:" << (b ? "enabled" : "disabled");
}

QVariantList DialController::allProfiles() const
{
    QVariantList result;
    const QStringList ids = m_profileManager->getProfileIds();
    for (const QString &id : ids) {
        const Profile *p = m_profileManager->getProfile(id);
        if (!p) continue;
        QVariantMap m;
        m[QStringLiteral("id")]      = id;
        m[QStringLiteral("name")]    = p->displayName;
        m[QStringLiteral("enabled")] = p->enabled ? 1 : 0;
        result.append(m);
    }
    return result;
}

void DialController::setProfileEnabled(const QString &profileId, int enabled)
{
    const Profile *p = m_profileManager->getProfile(profileId);
    if (!p) return;
    Profile updated = *p;
    updated.enabled = (enabled != 0);
    m_profileManager->saveProfile(updated);
    Q_EMIT profilesChanged();
}

// ── Settings window ───────────────────────────────────────────────────────────

void DialController::openSettings()
{
    Q_EMIT settingsRequested();
}

QVariantMap DialController::getProfileData(const QString &profileId) const
{
    const Profile *p = m_profileManager->getProfile(profileId);
    if (!p) return QVariantMap();
    return p->toJson().toVariantMap();
}

int DialController::saveProfileData(const QVariantMap &profileData)
{
    QJsonObject json = QJsonObject::fromVariantMap(profileData);
    Profile profile = Profile::fromJson(json);
    if (profile.id.isEmpty()) {
        qWarning() << "saveProfileData: profile has no id";
        return 0;
    }
    bool ok = m_profileManager->saveProfile(profile);
    if (ok) {
        // If the currently active profile was edited, refresh the controller state
        if (profile.id == m_currentProfileId) {
            updateCurrentFunction();
            Q_EMIT functionsChanged();
        }
    }
    return ok ? 1 : 0;
}

QString DialController::createNewProfile(const QString &displayName)
{
    return m_profileManager->createProfile(displayName);
}

int DialController::deleteProfile(const QString &profileId)
{
    return m_profileManager->deleteProfile(profileId) ? 1 : 0;
}

void DialController::reloadAllProfiles()
{
    m_profileManager->reloadProfiles();
    updateCurrentFunction();
    Q_EMIT functionsChanged();
}

QStringList DialController::getSupportedKeys() const
{
    // Ask the daemon for the authoritative key list
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.asus.dial"),
        QStringLiteral("/org/asus/dial"),
        QStringLiteral("org.asus.dial"),
        QStringLiteral("ListSupportedKeys")
    );
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 500);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        return reply.arguments().first().toStringList();
    }

    // Fallback when daemon is unreachable — return the common set
    return {
        QStringLiteral("a"),  QStringLiteral("b"),  QStringLiteral("c"),
        QStringLiteral("d"),  QStringLiteral("e"),  QStringLiteral("f"),
        QStringLiteral("g"),  QStringLiteral("h"),  QStringLiteral("i"),
        QStringLiteral("j"),  QStringLiteral("k"),  QStringLiteral("l"),
        QStringLiteral("m"),  QStringLiteral("n"),  QStringLiteral("o"),
        QStringLiteral("p"),  QStringLiteral("q"),  QStringLiteral("r"),
        QStringLiteral("s"),  QStringLiteral("t"),  QStringLiteral("u"),
        QStringLiteral("v"),  QStringLiteral("w"),  QStringLiteral("x"),
        QStringLiteral("y"),  QStringLiteral("z"),
        QStringLiteral("0"),  QStringLiteral("1"),  QStringLiteral("2"),
        QStringLiteral("3"),  QStringLiteral("4"),  QStringLiteral("5"),
        QStringLiteral("6"),  QStringLiteral("7"),  QStringLiteral("8"),
        QStringLiteral("9"),
        QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3"),
        QStringLiteral("F4"), QStringLiteral("F5"), QStringLiteral("F6"),
        QStringLiteral("F7"), QStringLiteral("F8"), QStringLiteral("F9"),
        QStringLiteral("F10"),QStringLiteral("F11"),QStringLiteral("F12"),
        QStringLiteral("bracketleft"), QStringLiteral("bracketright"),
        QStringLiteral("equal"), QStringLiteral("minus"),
        QStringLiteral("comma"), QStringLiteral("period"),
        QStringLiteral("semicolon"), QStringLiteral("apostrophe"),
        QStringLiteral("grave"), QStringLiteral("backslash"),
        QStringLiteral("slash"),
        QStringLiteral("Tab"), QStringLiteral("Return"),
        QStringLiteral("Escape"), QStringLiteral("space"),
        QStringLiteral("BackSpace"), QStringLiteral("Delete"),
        QStringLiteral("Insert"), QStringLiteral("Home"),
        QStringLiteral("End"), QStringLiteral("Prior"),
        QStringLiteral("Next"),
        QStringLiteral("left"), QStringLiteral("right"),
        QStringLiteral("up"), QStringLiteral("down"),
        QStringLiteral("ctrl"), QStringLiteral("shift"),
        QStringLiteral("alt"), QStringLiteral("super"),
        QStringLiteral("XF86AudioRaiseVolume"),
        QStringLiteral("XF86AudioLowerVolume"),
        QStringLiteral("XF86AudioMute"),
        QStringLiteral("XF86AudioPlay"),
        QStringLiteral("XF86AudioNext"),
        QStringLiteral("XF86AudioPrev"),
        QStringLiteral("XF86MonBrightnessUp"),
        QStringLiteral("XF86MonBrightnessDown"),
        QStringLiteral("XF86ZoomIn"),
        QStringLiteral("XF86ZoomOut")
    };
}

int DialController::saveProfileDataJson(const QString &jsonString)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "saveProfileDataJson: invalid JSON -" << err.errorString();
        return 0;
    }
    Profile profile = Profile::fromJson(doc.object());
    if (profile.id.isEmpty()) {
        qWarning() << "saveProfileDataJson: profile has no id";
        return 0;
    }
    bool ok = m_profileManager->saveProfile(profile);
    if (ok) {
        if (profile.id == m_currentProfileId) {
            updateCurrentFunction();
            Q_EMIT functionsChanged();
        }
    }
    return ok ? 1 : 0;
}

void DialController::testAction(const QVariantMap &actionData)
{
    QJsonObject json = QJsonObject::fromVariantMap(actionData);
    ActionConfig action = ActionConfig::fromJson(json);
    m_actionExecutor->executeAction(action, 1);
}

QString DialController::importProfile(const QString &filePath)
{
    // QML FileDialog returns file:// URIs on some platforms — strip the scheme.
    QString path = filePath;
    if (path.startsWith(QStringLiteral("file://")))
        path = path.mid(7);

    const QString id = m_profileManager->importProfile(path);
    if (!id.isEmpty())
        Q_EMIT profilesChanged();
    return id;
}

int DialController::exportProfile(const QString &profileId, const QString &filePath)
{
    QString path = filePath;
    if (path.startsWith(QStringLiteral("file://")))
        path = path.mid(7);

    // Ensure .json extension
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        path += QStringLiteral(".json");

    const bool ok = m_profileManager->exportProfile(profileId, path);
    return ok ? 1 : 0;
}

QVariantMap DialController::functionToVariantMap(const Function &func) const
{
    QVariantMap map;

    map[QStringLiteral("id")] = func.id;
    map[QStringLiteral("label")] = func.label;
    map[QStringLiteral("iconName")] = func.iconName;
    map[QStringLiteral("type")] = (func.type == Function::Type::Continuous) ? QStringLiteral("continuous") : QStringLiteral("discrete");
    map[QStringLiteral("unit")] = func.unit;
    map[QStringLiteral("suppressOverlay")] = func.suppressOverlay ? 1 : 0;

    if (func.minValue.has_value()) {
        map[QStringLiteral("minValue")] = func.minValue.value();
    }
    if (func.maxValue.has_value()) {
        map[QStringLiteral("maxValue")] = func.maxValue.value();
    }

    // Tell the overlay whether this function should keep the HUD hidden.
    // Automatic for scroll/zoom/sticky; also respected if the user explicitly
    // set suppressOverlay = true in the profile.
    static const QStringList kFocusEscapeKeys = {
        QStringLiteral("ZoomIn"), QStringLiteral("ZoomOut")
    };
    const bool isScroll =
        func.clockwiseAction.type        == ActionConfig::Type::MouseScroll ||
        func.counterClockwiseAction.type == ActionConfig::Type::MouseScroll;
    const bool isSticky =
        func.clockwiseAction.sticky || func.counterClockwiseAction.sticky;
    const bool needsEscape =
        func.suppressOverlay || isScroll || isSticky ||
        kFocusEscapeKeys.contains(func.clockwiseAction.keys) ||
        kFocusEscapeKeys.contains(func.counterClockwiseAction.keys);
    map[QStringLiteral("needsFocusEscape")] = needsEscape;

    return map;
}
