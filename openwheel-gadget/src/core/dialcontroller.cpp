// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include "dialcontroller.h"
#include "rotationhandler.h"
#include "actionexecutor.h"
#include "../profiles/profilemanager.h"
#include "../profiles/applicationmatcher.h"
#include "../dbus/dbusinterface.h"
#include <QDebug>

DialController::DialController(QObject *parent)
    : QObject(parent)
    , m_dbusInterface(std::make_unique<DBusInterface>())
    , m_profileManager(std::make_unique<ProfileManager>())
    , m_appMatcher(std::make_unique<ApplicationMatcher>())
    , m_actionExecutor(std::make_unique<ActionExecutor>())
    , m_rotationHandler(std::make_unique<RotationHandler>())
{
    setupConnections();
}

DialController::~DialController() = default;

void DialController::initialize()
{
    qDebug() << "Initializing DialController";

    // Load all profiles
    m_profileManager->loadProfiles();

    // Start monitoring window changes
    m_appMatcher->startMonitoring();

    qDebug() << "DialController initialized";
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

    // Profile manager connections
    connect(m_profileManager.get(), &ProfileManager::currentProfileChanged,
            this, [this]() {
                Q_EMIT profileChanged(currentProfileName());
                updateCurrentFunction();
                Q_EMIT functionsChanged();
            });

    // Rotation handler connections
    connect(m_rotationHandler.get(), &RotationHandler::actionTriggered,
            this, &DialController::onActionTriggered);
    connect(m_rotationHandler.get(), &RotationHandler::rotationTick,
            this, &DialController::rotationTick);
    connect(m_rotationHandler.get(), &RotationHandler::valueChanged,
            this, &DialController::valueChanged);
    connect(m_rotationHandler.get(), &RotationHandler::adjustingChanged,
            this, &DialController::adjustingChanged);
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
    // Could deactivate overlay here if desired
    // deactivate();
}

void DialController::onRotationChanged(int delta)
{
    qDebug() << "DialController: Rotation changed:" << delta;

    // If not active, just pass through to current function
    // If active (overlay showing), use rotation to select functions
    if (m_active) {
        // Rotate through functions
        if (delta > 0) {
            selectNext();
        } else if (delta < 0) {
            selectPrevious();
        }
    } else {
        // Pass rotation to handler for action execution
        m_rotationHandler->onRotation(delta);
    }
}

void DialController::onButtonPressed()
{
    qDebug() << "DialController: Button pressed";

    // Activate overlay
    activate();
}

void DialController::onButtonReleased()
{
    qDebug() << "DialController: Button released";

    // Deactivate overlay
    deactivate();
}

void DialController::onWindowChanged(const QString &windowClass,
                                      const QString &windowName,
                                      const QString &processName)
{
    qDebug() << "DialController: Window changed -" << windowClass << "/" << processName;

    // Find matching profile
    QString profileId = m_profileManager->findMatchingProfile(windowClass, windowName, processName);

    if (!profileId.isEmpty() && profileId != m_currentProfileId) {
        switchToProfile(profileId);
    }
}

void DialController::onActionTriggered(const ActionConfig &action, int repeatCount)
{
    qDebug() << "DialController: Executing action" << repeatCount << "times";

    m_actionExecutor->executeAction(action, repeatCount);
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
    m_rotationHandler->reset();

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

QVariantMap DialController::functionToVariantMap(const Function &func) const
{
    QVariantMap map;

    map[QStringLiteral("id")] = func.id;
    map[QStringLiteral("label")] = func.label;
    map[QStringLiteral("iconName")] = func.iconName;
    map[QStringLiteral("type")] = (func.type == Function::Type::Continuous) ? QStringLiteral("continuous") : QStringLiteral("discrete");
    map[QStringLiteral("unit")] = func.unit;

    if (func.minValue.has_value()) {
        map[QStringLiteral("minValue")] = func.minValue.value();
    }
    if (func.maxValue.has_value()) {
        map[QStringLiteral("maxValue")] = func.maxValue.value();
    }

    return map;
}
