// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include "rotationhandler.h"
#include <QTimer>
#include <QDebug>
#include <QtMath>

RotationHandler::RotationHandler(QObject *parent)
    : QObject(parent)
{
    m_velocityTimer.start();

    // Timer to detect when rotation has stopped
    m_adjustingTimer = new QTimer(this);
    m_adjustingTimer->setSingleShot(true);
    m_adjustingTimer->setInterval(ADJUSTING_TIMEOUT);

    connect(m_adjustingTimer, &QTimer::timeout, this, [this]() {
        if (m_isAdjusting) {
            m_isAdjusting = false;
            Q_EMIT adjustingChanged(false);
            qDebug() << "Stopped adjusting";
        }
    });
}

RotationHandler::~RotationHandler() = default;

void RotationHandler::setCurrentActions(const ActionConfig &clockwise,
                                         const ActionConfig &counterClockwise)
{
    m_clockwiseAction = clockwise;
    m_counterClockwiseAction = counterClockwise;

    // Use the threshold from the action if specified
    if (clockwise.rotationThreshold > 0) {
        m_baseThreshold = clockwise.rotationThreshold;
    } else if (counterClockwise.rotationThreshold > 0) {
        m_baseThreshold = counterClockwise.rotationThreshold;
    }

    // Use acceleration setting from action
    m_accelerationEnabled = clockwise.accelerationEnabled || counterClockwise.accelerationEnabled;

    qDebug() << "RotationHandler: threshold =" << m_baseThreshold
             << ", acceleration =" << m_accelerationEnabled;
}

void RotationHandler::onRotation(int deltaDegrees)
{
    if (deltaDegrees == 0) {
        return;
    }

    qDebug() << "Rotation delta:" << deltaDegrees << "degrees";

    // Update adjusting state
    updateAdjustingState();

    // Calculate velocity
    qint64 elapsed = m_velocityTimer.elapsed();
    m_velocityTimer.restart();

    if (elapsed > 0 && elapsed < 1000) {  // Only if reasonable time window
        qreal instantVelocity = qAbs(deltaDegrees) / (elapsed / 1000.0);
        m_currentVelocity = m_currentVelocity * (1.0 - VELOCITY_SMOOTHING)
                          + instantVelocity * VELOCITY_SMOOTHING;

        qDebug() << "Velocity:" << m_currentVelocity << "deg/sec (instant:" << instantVelocity << ")";
    }

    // Apply acceleration
    qreal effectiveThreshold = m_baseThreshold;
    if (m_accelerationEnabled) {
        qreal acceleration = calculateAcceleration(m_currentVelocity);
        effectiveThreshold = m_baseThreshold / acceleration;
        qDebug() << "Acceleration factor:" << acceleration << "-> effective threshold:" << effectiveThreshold;
    }

    // Accumulate rotation
    m_accumulatedRotation += deltaDegrees;
    m_currentValue += deltaDegrees;

    Q_EMIT valueChanged(m_currentValue);

    // Check if we should trigger actions
    int triggers = static_cast<int>(qAbs(m_accumulatedRotation) / effectiveThreshold);

    if (triggers > 0) {
        const ActionConfig &action = (m_accumulatedRotation > 0)
            ? m_clockwiseAction
            : m_counterClockwiseAction;

        qDebug() << "Triggering action" << triggers << "times";

        Q_EMIT actionTriggered(action, triggers);
        Q_EMIT rotationTick();

        // Keep remainder
        qreal sign = (m_accumulatedRotation > 0) ? 1.0 : -1.0;
        m_accumulatedRotation = sign * std::fmod(qAbs(m_accumulatedRotation), effectiveThreshold);

        qDebug() << "Accumulated remainder:" << m_accumulatedRotation;
    }
}

void RotationHandler::reset()
{
    m_accumulatedRotation = 0.0;
    m_currentVelocity = 0.0;
    m_currentValue = 0.0;
    m_velocityTimer.restart();

    if (m_isAdjusting) {
        m_isAdjusting = false;
        Q_EMIT adjustingChanged(false);
    }

    m_adjustingTimer->stop();

    qDebug() << "RotationHandler reset";
}

void RotationHandler::setAccelerationEnabled(int enabled)
{
    m_accelerationEnabled = enabled;
    qDebug() << "Acceleration" << (enabled ? "enabled" : "disabled");
}

void RotationHandler::setBaseThreshold(int degrees)
{
    m_baseThreshold = degrees;
    qDebug() << "Base threshold set to" << degrees << "degrees";
}

qreal RotationHandler::calculateAcceleration(qreal velocity) const
{
    if (velocity < SLOW_THRESHOLD) {
        // Slow rotation: no acceleration
        return 1.0;
    } else if (velocity < FAST_THRESHOLD) {
        // Medium rotation: linear interpolation
        qreal t = (velocity - SLOW_THRESHOLD) / (FAST_THRESHOLD - SLOW_THRESHOLD);
        return 1.0 + t * (MAX_ACCELERATION - 1.0);
    } else {
        // Fast rotation: maximum acceleration
        return MAX_ACCELERATION;
    }
}

void RotationHandler::updateAdjustingState()
{
    if (!m_isAdjusting) {
        m_isAdjusting = true;
        Q_EMIT adjustingChanged(true);
        qDebug() << "Started adjusting";
    }

    // Restart the timeout timer
    m_adjustingTimer->start();
}
