// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#pragma once

#include <QObject>
#include <QString>

/**
 * Detects the currently active application window and emits signals
 * when it changes. Uses KWindowSystem for window information.
 */
class ApplicationMatcher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentWindowClass READ currentWindowClass NOTIFY windowChanged)
    Q_PROPERTY(QString currentWindowName READ currentWindowName NOTIFY windowChanged)
    Q_PROPERTY(QString currentProcessName READ currentProcessName NOTIFY windowChanged)

public:
    explicit ApplicationMatcher(QObject *parent = nullptr);
    ~ApplicationMatcher() override;

    QString currentWindowClass() const { return m_currentWindowClass; }
    QString currentWindowName() const { return m_currentWindowName; }
    QString currentProcessName() const { return m_currentProcessName; }

    /**
     * Start monitoring window changes
     */
    void startMonitoring();

    /**
     * Stop monitoring window changes
     */
    void stopMonitoring();

Q_SIGNALS:
    void windowChanged(const QString &windowClass,
                       const QString &windowName,
                       const QString &processName);

private Q_SLOTS:
    void onActiveWindowChanged();

private:
    QString getProcessName(int pid) const;
    void updateCurrentWindow();

    QString m_currentWindowClass;
    QString m_currentWindowName;
    QString m_currentProcessName;
    int m_monitoring = 0;
};
