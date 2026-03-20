// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include "profilemanager.h"
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDebug>

ProfileManager::ProfileManager(QObject *parent)
    : QObject(parent)
{
}

ProfileManager::~ProfileManager() = default;

void ProfileManager::loadProfiles()
{
    m_profiles.clear();

    // Load from system data directories
    QStringList dataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &dataDir : dataDirs) {
        QString profilesDir = dataDir + QStringLiteral("/openwheel/profiles");
        if (QDir(profilesDir).exists()) {
            qDebug() << "Loading profiles from:" << profilesDir;
            loadProfilesFromDirectory(profilesDir);
        }
    }

    // Also check for profiles in the build directory (for development)
    QString devProfilesDir = QStringLiteral("./profiles");
    if (QDir(devProfilesDir).exists()) {
        qDebug() << "Loading development profiles from:" << devProfilesDir;
        loadProfilesFromDirectory(devProfilesDir);
    }

    // Find the default profile
    m_defaultProfileId = getDefaultProfileId();

    if (m_profiles.empty()) {
        qWarning() << "No profiles loaded!";
    } else {
        qDebug() << "Loaded" << m_profiles.size() << "profiles";
        qDebug() << "Default profile:" << m_defaultProfileId;
    }

    // Set current profile to default if not set
    if (m_currentProfileId.isEmpty()) {
        setCurrentProfile(m_defaultProfileId);
    }

    Q_EMIT profilesLoaded();
}

void ProfileManager::loadProfilesFromDirectory(const QString &dirPath)
{
    QDir dir(dirPath);
    QStringList filters;
    filters << QStringLiteral("*.json");

    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &fileInfo : files) {
        loadProfile(fileInfo.absoluteFilePath());
    }
}

bool ProfileManager::loadProfile(const QString &filePath)
{
    Profile profile = Profile::fromFile(filePath);

    if (profile.id.isEmpty()) {
        qWarning() << "Failed to load profile from:" << filePath;
        return false;
    }

    qDebug() << "Loaded profile:" << profile.id << "(" << profile.displayName << ")";

    QString id = profile.id;
    m_profiles[id] = QSharedPointer<Profile>::create(std::move(profile));
    return true;
}

const Profile* ProfileManager::getProfile(const QString &profileId) const
{
    auto it = m_profiles.find(profileId);
    if (it != m_profiles.end()) {
        return it.value().get();
    }
    return nullptr;
}

Profile* ProfileManager::getProfile(const QString &profileId)
{
    auto it = m_profiles.find(profileId);
    if (it != m_profiles.end()) {
        return it.value().get();
    }
    return nullptr;
}

const Profile* ProfileManager::getCurrentProfile() const
{
    return getProfile(m_currentProfileId);
}

Profile* ProfileManager::getCurrentProfile()
{
    return getProfile(m_currentProfileId);
}

void ProfileManager::setCurrentProfile(const QString &profileId)
{
    if (profileId.isEmpty()) {
        return;
    }

    if (!m_profiles.contains(profileId)) {
        qWarning() << "Profile not found:" << profileId;
        return;
    }

    if (m_currentProfileId != profileId) {
        m_currentProfileId = profileId;
        qDebug() << "Switched to profile:" << profileId;
        Q_EMIT currentProfileChanged(profileId);
    }
}

QString ProfileManager::findMatchingProfile(const QString &windowClass,
                                             const QString &windowTitle,
                                             const QString &processName) const
{
    // First try to find a specific matching profile
    for (auto it = m_profiles.begin(); it != m_profiles.end(); ++it) {
        const Profile *profile = it.value().get();
        if (!profile->isDefault && profile->matches(windowClass, windowTitle, processName)) {
            qDebug() << "Found matching profile:" << profile->id
                     << "for window:" << windowClass << "/" << processName;
            return profile->id;
        }
    }

    // Fall back to default profile
    qDebug() << "No specific match, using default profile for window:" << windowClass;
    return m_defaultProfileId;
}

QStringList ProfileManager::getProfileIds() const
{
    QStringList ids;
    ids = m_profiles.keys();
    return ids;
}

QString ProfileManager::currentProfileName() const
{
    const Profile *profile = getCurrentProfile();
    return profile ? profile->displayName : QString();
}

QString ProfileManager::getDefaultProfileId() const
{
    // Find the profile marked as default
    for (auto it = m_profiles.begin(); it != m_profiles.end(); ++it) {
        if (it.value()->isDefault) {
            return it.key();
        }
    }

    // If no default is marked, use "system-default" if it exists
    if (m_profiles.contains(QStringLiteral("system-default"))) {
        return QStringLiteral("system-default");
    }

    // Fall back to any profile
    if (!m_profiles.empty()) {
        return m_profiles.begin().key();
    }

    return QString();
}
