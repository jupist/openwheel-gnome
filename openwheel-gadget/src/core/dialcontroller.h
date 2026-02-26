// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#pragma once

#include <QObject>
#include <QVariantList>
#include <memory>
#include <array>
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

    Q_INVOKABLE void activate();
    Q_INVOKABLE void deactivate();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void selectNext();
    Q_INVOKABLE void selectPrevious();
    Q_INVOKABLE void confirmSelection();

Q_SIGNALS:
    void activeChanged(int active);
    void profileChanged(const QString &profileName);
    void selectionChanged(int index);
    void functionsChanged();
    void functionChanged();
    void valueChanged(qreal value);
    void adjustingChanged(int adjusting);
    void rotationTick();

private Q_SLOTS:
    void onRotationChanged(int delta);
    void onButtonPressed();
    void onButtonReleased();
    void onWindowChanged(const QString &windowClass,
                         const QString &windowName,
                         const QString &processName);
    void onActionTriggered(const ActionConfig &action, int repeatCount);

private:
    void setupConnections();
    void switchToProfile(const QString &profileId);
    void updateCurrentFunction();
    QVariantMap functionToVariantMap(const Function &func) const;

    std::unique_ptr<DBusInterface> m_dbusInterface;
    std::unique_ptr<ProfileManager> m_profileManager;
    std::unique_ptr<ApplicationMatcher> m_appMatcher;
    std::unique_ptr<ActionExecutor> m_actionExecutor;
    std::unique_ptr<RotationHandler> m_rotationHandler;

    int m_active = 0;
    int m_selectedIndex = 0;
    QString m_currentProfileId;
};
