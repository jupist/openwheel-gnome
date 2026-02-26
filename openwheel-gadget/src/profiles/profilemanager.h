// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#pragma once

#include "profile.h"
#include <QObject>
#include <QString>
#include <memory>
#include <map>

/**
 * Manages loading, caching, and switching between profiles
 */
class ProfileManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentProfileId READ currentProfileId NOTIFY currentProfileChanged)
    Q_PROPERTY(QString currentProfileName READ currentProfileName NOTIFY currentProfileChanged)

public:
    explicit ProfileManager(QObject *parent = nullptr);
    ~ProfileManager() override;

    /**
     * Load all profiles from standard directories
     */
    void loadProfiles();

    /**
     * Load a single profile from a file
     */
    bool loadProfile(const QString &filePath);

    /**
     * Get a profile by ID
     */
    const Profile* getProfile(const QString &profileId) const;
    Profile* getProfile(const QString &profileId);

    /**
     * Get the current active profile
     */
    const Profile* getCurrentProfile() const;
    Profile* getCurrentProfile();

    /**
     * Switch to a specific profile
     */
    void setCurrentProfile(const QString &profileId);

    /**
     * Find the best matching profile for the given window
     */
    QString findMatchingProfile(const QString &windowClass,
                                const QString &windowTitle,
                                const QString &processName) const;

    /**
     * Get all loaded profile IDs
     */
    QStringList getProfileIds() const;

    QString currentProfileId() const { return m_currentProfileId; }
    QString currentProfileName() const;

Q_SIGNALS:
    void currentProfileChanged(const QString &profileId);
    void profilesLoaded();

private:
    void loadProfilesFromDirectory(const QString &dirPath);
    QString getDefaultProfileId() const;

    std::map<QString, std::unique_ptr<Profile>> m_profiles;
    QString m_currentProfileId;
    QString m_defaultProfileId;
};
