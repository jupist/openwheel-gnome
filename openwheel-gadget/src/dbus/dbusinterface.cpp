// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include "dbusinterface.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>

DBusInterface::DBusInterface(QObject *parent)
    : QObject(parent)
    , m_connection(QDBusConnection::sessionBus())
{
    connectToDaemon();
    setupSignalHandlers();
}

DBusInterface::~DBusInterface() = default;

void DBusInterface::connectToDaemon()
{
    if (!m_connection.isConnected()) {
        qWarning() << "Failed to connect to D-Bus session bus";
        m_connected = false;
        Q_EMIT connectionStatusChanged(false);
        return;
    }

    m_connected = true;
    Q_EMIT connectionStatusChanged(true);
    qDebug() << "Connected to D-Bus session bus";
}

void DBusInterface::setupSignalHandlers()
{
    // Connect to org.asus.dial signals from the daemon
    bool rotateConnected = m_connection.connect(
        QStringLiteral("org.asus.dial"),         // service
        QStringLiteral("/org/asus/dial"),        // path
        QStringLiteral("org.asus.dial"),         // interface
        QStringLiteral("Rotate"),                // signal name
        this,
        SLOT(onDialRotate(QDBusMessage))
    );

    bool pressConnected = m_connection.connect(
        QStringLiteral("org.asus.dial"),         // service
        QStringLiteral("/org/asus/dial"),        // path
        QStringLiteral("org.asus.dial"),         // interface
        QStringLiteral("Press"),                 // signal name
        this,
        SLOT(onDialPress(QDBusMessage))
    );

    if (rotateConnected && pressConnected) {
        qDebug() << "Successfully connected to openwheel-daemon signals";
    } else {
        qWarning() << "Failed to connect to openwheel-daemon signals";
        qWarning() << "Rotate connected:" << rotateConnected;
        qWarning() << "Press connected:" << pressConnected;
    }
}

void DBusInterface::onDialRotate(const QDBusMessage &message)
{
    // The daemon sends rotation delta as an integer
    QList<QVariant> args = message.arguments();
    if (!args.isEmpty()) {
        int delta = args.at(0).toInt();
        qDebug() << "Received rotation delta:" << delta;
        Q_EMIT rotationChanged(delta);
    }
}

void DBusInterface::onDialPress(const QDBusMessage &message)
{
    // The daemon sends button state as an integer (0 = up, 1 = down)
    QList<QVariant> args = message.arguments();
    if (!args.isEmpty()) {
        int buttonState = args.at(0).toInt();
        bool isPressed = (buttonState == 1);

        qDebug() << "Received button state:" << buttonState << "(" << (isPressed ? "pressed" : "released") << ")";

        // Emit appropriate signal based on state change
        if (isPressed && !m_lastButtonState) {
            Q_EMIT buttonPressed();
        } else if (!isPressed && m_lastButtonState) {
            Q_EMIT buttonReleased();
        }

        m_lastButtonState = isPressed;
    }
}
