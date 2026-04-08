// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#pragma once

#include <QObject>
#include <QVariantList>
#include <QScopedPointer>
#include <QElapsedTimer>
#include <QTimer>
#include <QSettings>
#include "../profiles/profile.h"

class ProfileManager;
class ApplicationMatcher;
class ActionExecutor;
class RotationHandler;
class DBusInterface;
struct Function;

/**
 * Main controller that coordinates all dial functionality
 * This class is exposed to QML for UI integration
 */
class DialController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QString currentProfileName READ currentProfileName NOTIFY profileChanged)
    Q_PROPERTY(int selectedFunctionIndex READ selectedFunctionIndex NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList currentFunctions READ currentFunctions NOTIFY functionsChanged)
    Q_PROPERTY(int isAdjusting READ isAdjusting NOTIFY adjustingChanged)
    Q_PROPERTY(qreal currentValue READ currentValue NOTIFY valueChanged)
    Q_PROPERTY(QString currentUnit READ currentUnit NOTIFY functionChanged)
    Q_PROPERTY(qreal currentMinValue READ currentMinValue NOTIFY functionChanged)
    Q_PROPERTY(qreal currentMaxValue READ currentMaxValue NOTIFY functionChanged)
    // Profile picker
    Q_PROPERTY(QVariantList availableProfiles READ availableProfiles NOTIFY profilesChanged)
    Q_PROPERTY(int pickerActive READ isPickerActive NOTIFY pickerActiveChanged)
    Q_PROPERTY(int pickerIndex READ pickerIndex NOTIFY pickerIndexChanged)
    // Global behaviour (int not bool — X11 headers define Bool as a macro)
    Q_PROPERTY(int pressToActivate READ pressToActivate WRITE setPressToActivate NOTIFY pressToActivateChanged)
    // Daemon systemd user-service autostart (default: disabled)
    Q_PROPERTY(int daemonAutostart READ daemonAutostart WRITE setDaemonAutostart NOTIFY daemonAutostartChanged)

public:
    explicit DialController(QObject *parent = nullptr);
    ~DialController() override;

    /**
     * Initialize the controller (load profiles, start monitoring, etc.)
     */
    void initialize();

    int isActive() const { return m_active; }
    QString currentProfileName() const;
    int selectedFunctionIndex() const { return m_selectedIndex; }
    QVariantList currentFunctions() const;
    int isAdjusting() const;
    qreal currentValue() const;
    QString currentUnit() const;
    qreal currentMinValue() const;
    qreal currentMaxValue() const;

    // Profile picker
    QVariantList availableProfiles() const;
    int isPickerActive() const { return m_pickerActive ? 1 : 0; }
    int pickerIndex() const { return m_pickerIndex; }

    // Global behaviour (int not bool — X11 headers define Bool as a macro)
    int pressToActivate() const { return m_pressToActivate ? 1 : 0; }
    void setPressToActivate(int enabled);

    // Daemon systemd autostart
    int daemonAutostart() const;
    void setDaemonAutostart(int enabled);

    Q_INVOKABLE void activate();
    Q_INVOKABLE void deactivate();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void selectNext();
    Q_INVOKABLE void selectPrevious();
    Q_INVOKABLE void confirmSelection();

    // Manual profile picker (long-press).
    Q_INVOKABLE void openProfilePicker();
    Q_INVOKABLE void closeProfilePicker();
    Q_INVOKABLE void confirmProfilePicker();   // apply pickerIndex selection
    Q_INVOKABLE void setActiveProfile(const QString &profileId);

    // Settings window.
    Q_INVOKABLE void openSettings();

    // Profile CRUD exposed to QML settings window.
    Q_INVOKABLE QVariantMap getProfileData(const QString &profileId) const;
    Q_INVOKABLE int saveProfileData(const QVariantMap &profileData);  // returns 1 on success
    // Preferred path: QML passes JSON.stringify(profileData) to avoid
    // QVariantMap→QJsonObject round-trip issues with nested arrays.
    Q_INVOKABLE int saveProfileDataJson(const QString &jsonString);
    Q_INVOKABLE QString createNewProfile(const QString &displayName);
    Q_INVOKABLE int deleteProfile(const QString &profileId);           // returns 1 on success
    Q_INVOKABLE void reloadAllProfiles();

    // Returns ALL profiles (including disabled) for the settings UI.
    Q_INVOKABLE QVariantList allProfiles() const;
    // Toggle a profile's enabled state and persist it.
    Q_INVOKABLE void setProfileEnabled(const QString &profileId, int enabled);

    // Returns key name strings accepted by the daemon's InjectKey method.
    Q_INVOKABLE QStringList getSupportedKeys() const;

    // Fires a single action immediately (used by the "Test" button in the settings UI).
    Q_INVOKABLE void testAction(const QVariantMap &actionData);

Q_SIGNALS:
    void activeChanged(int active);
    void profileChanged(const QString &profileName);
    void selectionChanged(int index);
    void functionsChanged();
    void functionChanged();
    void valueChanged(qreal value);
    void adjustingChanged(int adjusting);
    void rotationTick();
    void functionCycled();
    void profilesChanged();
    void settingsRequested();
    void pickerActiveChanged(int active);
    void pickerIndexChanged(int index);
    void pressToActivateChanged();  // bool arg omitted — X11 defines Bool as a macro
    void daemonAutostartChanged();
    void focusEscapeRequired();     // overlay should briefly lower itself

private Q_SLOTS:
    void onRotationChanged(int delta);
    void onButtonPressed();
    void onButtonReleased();
    void onWindowChanged(const QString &windowClass,
                         const QString &windowName,
                         const QString &processName);
    void onProfileIdRequested(const QString &profileId);
    void onActionTriggered(const ActionConfig &action, int repeatCount);

private:
    void setupConnections();
    void switchToProfile(const QString &profileId);
    void updateCurrentFunction();
    void persistSelectedProfile(const QString &profileId);
    QString loadPersistedProfile() const;
    QVariantMap functionToVariantMap(const Function &func) const;
    void executeClickAction();       // run the current function's click action
    QStringList enabledProfileIds() const; // enabled profiles in load order (used by picker)

    QScopedPointer<DBusInterface> m_dbusInterface;
    QScopedPointer<ProfileManager> m_profileManager;
    QScopedPointer<ApplicationMatcher> m_appMatcher;
    QScopedPointer<ActionExecutor> m_actionExecutor;
    QScopedPointer<RotationHandler> m_rotationHandler;

    int m_active = 0;
    int m_selectedIndex = 0;
    QString m_currentProfileId;

    // Profile picker state
    int m_pickerActive = 0;
    int m_pickerIndex = 0;
    int m_pickerTickAccum = 0;   // accumulates raw ticks; picker advances every 3
    QTimer *m_longPressTimer = nullptr;
    static constexpr int LONG_PRESS_MS = 600;

    // Press-to-activate state
    bool m_buttonHeld = false;           // true while the dial button is held
    bool m_rotatedDuringHold = false;    // any rotation happened during this hold

    QElapsedTimer m_buttonClickTimer;
    int m_clickCount = 0;
    QTimer *m_clickDecisionTimer = nullptr;
    static constexpr int DOUBLE_CLICK_INTERVAL = 400; // ms

    // Global behaviour flag — persisted in QSettings.
    bool m_pressToActivate = true;
};
