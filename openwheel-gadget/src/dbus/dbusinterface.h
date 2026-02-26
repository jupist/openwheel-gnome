// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#pragma once

#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>

/**
 * D-Bus interface to the openwheel-daemon
 * Listens for rotation and button press signals from org.asus.dial
 */
class DBusInterface : public QObject
{
    Q_OBJECT

public:
    explicit DBusInterface(QObject *parent = nullptr);
    ~DBusInterface() override;

    int isConnected() const { return m_connected; }

Q_SIGNALS:
    void rotationChanged(int delta);
    void buttonPressed();
    void buttonReleased();
    void connectionStatusChanged(int isConnected);

private Q_SLOTS:
    void onDialRotate(const QDBusMessage &message);
    void onDialPress(const QDBusMessage &message);

private:
    void connectToDaemon();
    void setupSignalHandlers();

    QDBusConnection m_connection;
    int m_connected = 0;
    int m_lastButtonState = 0;  // Track button state for press/release detection
};
