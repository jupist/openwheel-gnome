// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#pragma once

#include <QObject>
#include <QString>

/**
 * Previously detected the active application window via KWindowSystem.
 * Auto window detection has been dropped in favour of manual profile
 * selection (long-press the dial → rotate to pick a profile → click to
 * confirm). This class is kept as a stub that exposes the same interface
 * so the rest of the code does not need changing.
 *
 * If HAVE_KF6 is defined at build time (KDE Plasma), the KWindowSystem
 * path can be re-enabled in a future revision.
 */
class ApplicationMatcher : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationMatcher(QObject *parent = nullptr);
    ~ApplicationMatcher() override;

    // Lifecycle — kept for API compatibility; no-ops on GNOME.
    void startMonitoring();
    void stopMonitoring();

    // Programmatic override: switch to a named profile ID.
    // Emits windowChanged with empty class/title so DialController
    // treats it as a manual switch rather than an auto-detected one.
    void setCurrentProfileById(const QString &profileId);

Q_SIGNALS:
    // Emitted when the active application changes (KDE) or when the user
    // manually selects a profile (GNOME manual mode).
    // profileId carries the target profile when set via setCurrentProfileById();
    // on KDE it remains empty and class/name are used for matching.
    void windowChanged(const QString &windowClass,
                       const QString &windowName,
                       const QString &processName);

    // Emitted only by setCurrentProfileById() — a direct profile ID jump
    // that bypasses pattern matching in ProfileManager.
    void profileIdRequested(const QString &profileId);

private:
    bool m_monitoring = false;
};
