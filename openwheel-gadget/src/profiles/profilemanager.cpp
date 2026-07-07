// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include "profilemanager.h"
#include <algorithm>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

ProfileManager::ProfileManager(QObject *parent)
    : QObject(parent)
{
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(200);
    connect(&m_reloadTimer, &QTimer::timeout, this, [this]() {
        qDebug() << "Hot reloading profiles due to filesystem change...";
        reloadProfiles();
    });

    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &ProfileManager::onWatcherTriggered);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &ProfileManager::onWatcherTriggered);
}

void ProfileManager::onWatcherTriggered()
{
    m_reloadTimer.start();
}

ProfileManager::~ProfileManager() = default;

void ProfileManager::loadProfiles()
{
    m_profiles.clear();

    // Load development / bundled profiles first (lowest priority).
    // These are overridden by anything in the user's data dir.
    QString devProfilesDir = QStringLiteral("./profiles");
    if (QDir(devProfilesDir).exists()) {
        qDebug() << "Loading development profiles from:" << devProfilesDir;
        loadProfilesFromDirectory(devProfilesDir);
    }

    // Load from system then user data directories last so user edits win.
    // QStandardPaths returns dirs in priority order: ~/.local/share first,
    // then /usr/local/share, then /usr/share.  We reverse the list so that
    // higher-priority paths are loaded last and overwrite lower-priority ones.
    QStringList dataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    std::reverse(dataDirs.begin(), dataDirs.end());
    for (const QString &dataDir : dataDirs) {
        QString profilesDir = dataDir + QStringLiteral("/openwheel/profiles");
        if (QDir(profilesDir).exists()) {
            qDebug() << "Loading profiles from:" << profilesDir;
            loadProfilesFromDirectory(profilesDir);
        }
    }

    // Ensure the writable user profile directory exists and is watched for new files
    QString userDir = userProfileDir();
    if (QDir().mkpath(userDir) && !m_watcher.directories().contains(userDir)) {
        m_watcher.addPath(userDir);
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
    if (!m_watcher.directories().contains(dirPath)) {
        m_watcher.addPath(dirPath);
    }

    QStringList filters;
    filters << QStringLiteral("*.json");

    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &fileInfo : files) {
        QString absPath = fileInfo.absoluteFilePath();
        if (!m_watcher.files().contains(absPath)) {
            m_watcher.addPath(absPath);
        }
        loadProfile(absPath);
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

QString ProfileManager::userProfileDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/openwheel/profiles");
}

bool ProfileManager::saveProfile(const Profile &profile)
{
    if (profile.id.isEmpty()) {
        qWarning() << "saveProfile: empty profile id";
        return false;
    }

    QString filePath = userProfileDir() + QStringLiteral("/") + profile.id + QStringLiteral(".json");
    if (!profile.saveToFile(filePath)) {
        return false;
    }

    // Update in-memory cache
    const QString id = profile.id;
    m_profiles[id] = QSharedPointer<Profile>::create(profile);
    Q_EMIT profilesChanged();
    return true;
}

QString ProfileManager::createProfile(const QString &displayName)
{
    // Generate a filesystem-safe slug from the display name
    QString id = displayName.toLower()
                     .replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")),
                              QStringLiteral("-"))
                     .remove(QRegularExpression(QStringLiteral("^-|-$")));
    if (id.isEmpty()) id = QStringLiteral("profile");

    // Ensure uniqueness
    QString baseId = id;
    for (int suffix = 2; m_profiles.contains(id); ++suffix) {
        id = baseId + QStringLiteral("-") + QString::number(suffix);
    }

    Profile newProfile;
    newProfile.id = id;
    newProfile.displayName = displayName;
    newProfile.isDefault = false;

    // Seed with a default Volume function so the dial does something immediately
    Function volFunc;
    volFunc.id = QStringLiteral("volume");
    volFunc.label = QStringLiteral("Volume");
    volFunc.iconName = QStringLiteral("audio-volume-high");
    volFunc.type = Function::Type::Continuous;
    volFunc.unit = QStringLiteral("%");
    volFunc.minValue = 0.0;
    volFunc.maxValue = 100.0;
    volFunc.clockwiseAction.type = ActionConfig::Type::Keyboard;
    volFunc.clockwiseAction.keys = QStringLiteral("XF86AudioRaiseVolume");
    volFunc.counterClockwiseAction.type = ActionConfig::Type::Keyboard;
    volFunc.counterClockwiseAction.keys = QStringLiteral("XF86AudioLowerVolume");
    newProfile.functions.append(volFunc);
    newProfile.defaultMenuLayout.append(QStringLiteral("volume"));
    newProfile.defaultFunctionId = QStringLiteral("volume");

    if (!saveProfile(newProfile)) {
        return QString();
    }

    qDebug() << "Created new profile:" << id;
    return id;
}

bool ProfileManager::deleteProfile(const QString &profileId)
{
    const Profile *p = getProfile(profileId);
    if (!p) {
        qWarning() << "deleteProfile: profile not found:" << profileId;
        return false;
    }
    if (p->isDefault) {
        qWarning() << "deleteProfile: cannot delete default/system profile:" << profileId;
        return false;
    }

    // Only delete files from the user profile directory
    QString filePath = userProfileDir() + QStringLiteral("/") + profileId + QStringLiteral(".json");
    if (QFile::exists(filePath)) {
        if (!QFile::remove(filePath)) {
            qWarning() << "deleteProfile: failed to remove file:" << filePath;
            return false;
        }
    } else {
        qWarning() << "deleteProfile: profile is not a user profile (no file in user dir):" << profileId;
        return false;
    }

    m_profiles.remove(profileId);

    // Fall back to default if current profile was deleted
    if (m_currentProfileId == profileId) {
        m_defaultProfileId = getDefaultProfileId();
        setCurrentProfile(m_defaultProfileId);
    }

    Q_EMIT profilesChanged();
    return true;
}

void ProfileManager::reloadProfiles()
{
    const QString savedId = m_currentProfileId;
    m_currentProfileId.clear();
    loadProfiles();
    if (!savedId.isEmpty() && m_profiles.contains(savedId)) {
        setCurrentProfile(savedId);
    }
    Q_EMIT profilesChanged();
}

QString ProfileManager::importProfile(const QString &filePath)
{
    Profile imported = Profile::fromFile(filePath);
    if (imported.id.isEmpty()) {
        qWarning() << "importProfile: failed to parse" << filePath;
        return QString();
    }

    // Protect system profiles — reassign a new ID if there is a collision.
    const Profile *existing = getProfile(imported.id);
    if (existing && existing->isDefault) {
        QString base = imported.id + QStringLiteral("-imported");
        imported.id = base;
        for (int i = 2; m_profiles.contains(imported.id); ++i)
            imported.id = base + QStringLiteral("-") + QString::number(i);
        qDebug() << "importProfile: ID collision with system profile, using" << imported.id;
    }

    // System/default flag must never be set by an imported file.
    imported.isDefault = false;

    if (!saveProfile(imported)) {
        qWarning() << "importProfile: saveProfile failed for" << imported.id;
        return QString();
    }

    qDebug() << "Imported profile:" << imported.id;
    Q_EMIT profilesChanged();
    return imported.id;
}

bool ProfileManager::exportProfile(const QString &profileId, const QString &filePath) const
{
    const Profile *p = getProfile(profileId);
    if (!p) {
        qWarning() << "exportProfile: profile not found:" << profileId;
        return false;
    }
    return p->saveToFile(filePath);
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
