// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#pragma once

#include "profile.h"
#include <QObject>
#include <QString>
#include <QSharedPointer>

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

    /**
     * Save a profile to the user's profile directory.
     * System profiles (isDefault) cannot be overwritten.
     */
    bool saveProfile(const Profile &profile);

    /**
     * Create a new user profile with the given display name.
     * Returns the new profile's ID, or an empty string on failure.
     */
    QString createProfile(const QString &displayName);

    /**
     * Delete a user profile by ID. System/default profiles are protected.
     * Returns true on success.
     */
    bool deleteProfile(const QString &profileId);

    /**
     * Reload all profiles from disk, preserving the current selection.
     */
    void reloadProfiles();

    /**
     * Import a profile from an external JSON file.
     * The profile ID is taken from the file; if it conflicts with an existing
     * user profile the user copy is overwritten. System profiles are protected.
     * Returns the imported profile ID, or an empty string on failure.
     */
    QString importProfile(const QString &filePath);

    /**
     * Export a profile to an external JSON file.
     * Returns true on success.
     */
    bool exportProfile(const QString &profileId, const QString &filePath) const;

    /**
     * Returns the writable user profile directory path.
     */
    QString userProfileDir() const;

Q_SIGNALS:
    void currentProfileChanged(const QString &profileId);
    void profilesLoaded();
    void profilesChanged();

private:
    void loadProfilesFromDirectory(const QString &dirPath);
    QString getDefaultProfileId() const;

    QHash<QString, QSharedPointer<Profile>> m_profiles;
    QString m_currentProfileId;
    QString m_defaultProfileId;
};
