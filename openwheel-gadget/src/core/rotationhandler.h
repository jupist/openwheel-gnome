// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#pragma once

#include "../profiles/profile.h"
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

/**
 * Handles dial rotation input and converts it to actions
 * Includes acceleration curve for faster rotation
 */
class RotationHandler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal currentValue READ currentValue NOTIFY valueChanged)
    Q_PROPERTY(int isAdjusting READ isAdjusting NOTIFY adjustingChanged)

public:
    explicit RotationHandler(QObject *parent = nullptr);
    ~RotationHandler() override;

    /**
     * Set the current function's actions
     */
    void setCurrentActions(const ActionConfig &clockwise,
                           const ActionConfig &counterClockwise);

    /**
     * Process a rotation delta from the dial
     */
    void onRotation(int deltaDegrees);

    /**
     * Reset accumulated rotation
     */
    void reset();

    /**
     * Enable/disable acceleration
     */
    void setAccelerationEnabled(int enabled);

    /**
     * Set base rotation threshold (degrees per action trigger)
     */
    void setBaseThreshold(int degrees);

    qreal currentValue() const { return m_currentValue; }
    int isAdjusting() const { return m_isAdjusting; }

Q_SIGNALS:
    void actionTriggered(const ActionConfig &action, int repeatCount);
    void rotationTick();
    void valueChanged(qreal value);
    void adjustingChanged(int isAdjusting);

private:
    qreal calculateAcceleration(qreal velocity) const;
    void updateAdjustingState();

    ActionConfig m_clockwiseAction;
    ActionConfig m_counterClockwiseAction;

    qreal m_accumulatedRotation = 0.0;
    QElapsedTimer m_velocityTimer;
    qint64 m_lastRotationTime = 0;
    qreal m_currentVelocity = 0.0;

    int m_accelerationEnabled = 1;
    int m_baseThreshold = 15;

    qreal m_currentValue = 0.0;
    int m_isAdjusting = 0;
    QTimer *m_adjustingTimer = nullptr;

    static constexpr qreal VELOCITY_SMOOTHING = 0.3;
    static constexpr qreal SLOW_THRESHOLD = 30.0;   // deg/sec
    static constexpr qreal FAST_THRESHOLD = 120.0;  // deg/sec
    static constexpr qreal MAX_ACCELERATION = 4.0;
    static constexpr int ADJUSTING_TIMEOUT = 500;   // ms
};
