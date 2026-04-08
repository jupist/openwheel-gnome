// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include "applicationmatcher.h"
#include <QDebug>

ApplicationMatcher::ApplicationMatcher(QObject *parent)
    : QObject(parent)
{
}

ApplicationMatcher::~ApplicationMatcher()
{
    stopMonitoring();
}

void ApplicationMatcher::startMonitoring()
{
    if (m_monitoring) return;
    m_monitoring = true;
    // Auto window detection is not implemented on GNOME. Manual profile
    // selection via the overlay long-press is used instead.
    qDebug() << "ApplicationMatcher: manual profile selection mode (no auto window detection)";
}

void ApplicationMatcher::stopMonitoring()
{
    m_monitoring = false;
}

void ApplicationMatcher::setCurrentProfileById(const QString &profileId)
{
    if (profileId.isEmpty()) return;
    qDebug() << "ApplicationMatcher: manual profile switch requested ->" << profileId;
    Q_EMIT profileIdRequested(profileId);
}
