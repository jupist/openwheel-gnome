// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors
//
// Smoke test for M4 profile CRUD:
//   1. createProfile()   — writes a new JSON file to the user profile dir
//   2. getProfileData()  — reads it back as QVariantMap
//   3. saveProfile()     — updates it in-place
//   4. deleteProfile()   — removes the file
//
// Run: ./build/bin/smoke_settings  (no GUI required)

#include <QCoreApplication>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

#include "profiles/profilemanager.h"
#include "profiles/profile.h"

static QTextStream out(stdout);

#define CHECK(cond, msg) \
    do { if (!(cond)) { out << "FAIL: " << msg << Qt::endl; return 1; } } while(0)

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("openwheel-test"));
    QCoreApplication::setApplicationName(QStringLiteral("smoke_settings"));

    // Use a temp dir for user data so we don't litter ~/.local
    const QString tmpDir = QDir::tempPath() + QStringLiteral("/openwheel-smoke-settings");
    QDir().mkpath(tmpDir + QStringLiteral("/openwheel/profiles"));

    // Override GenericDataLocation by setting XDG_DATA_HOME
    qputenv("XDG_DATA_HOME", tmpDir.toUtf8());

    ProfileManager pm;
    pm.loadProfiles();

    out << "=== M4 profile CRUD smoke test ===" << Qt::endl;

    // ── 1. createProfile ─────────────────────────────────────────────────────
    const QString newId = pm.createProfile(QStringLiteral("Test App"));
    CHECK(!newId.isEmpty(), "createProfile returned empty id");
    out << "Created profile id: " << newId << Qt::endl;

    const Profile *p = pm.getProfile(newId);
    CHECK(p != nullptr, "getProfile returned null for new profile");
    CHECK(p->displayName == QStringLiteral("Test App"), "displayName mismatch");
    CHECK(!p->functions.isEmpty(), "new profile has no functions");
    out << "Profile loaded: " << p->displayName << " (" << p->functions.size() << " function(s))" << Qt::endl;

    // Verify the file was written
    const QString expectedFile = pm.userProfileDir() + QStringLiteral("/") + newId + QStringLiteral(".json");
    CHECK(QFile::exists(expectedFile), "profile JSON file not written to disk");
    out << "File exists: " << expectedFile << Qt::endl;

    // ── 2. saveProfile — modify and persist ──────────────────────────────────
    Profile copy = *p;
    copy.displayName = QStringLiteral("Test App (edited)");
    CHECK(pm.saveProfile(copy), "saveProfile failed");

    const Profile *updated = pm.getProfile(newId);
    CHECK(updated != nullptr, "getProfile null after save");
    CHECK(updated->displayName == QStringLiteral("Test App (edited)"), "displayName not updated");
    out << "Updated display name: " << updated->displayName << Qt::endl;

    // ── 3. toJson round-trip ─────────────────────────────────────────────────
    QJsonObject json = updated->toJson();
    Profile roundTripped = Profile::fromJson(json);
    CHECK(roundTripped.id == newId, "round-trip id mismatch");
    CHECK(roundTripped.displayName == QStringLiteral("Test App (edited)"), "round-trip displayName mismatch");
    CHECK(!roundTripped.functions.isEmpty(), "round-trip lost functions");
    // clickAction should be preserved (M4 fix: Function::toJson() now includes clickAction)
    const Function &func = roundTripped.functions.first();
    CHECK(func.clickAction.type == ActionConfig::Type::Keyboard, "clickAction type lost in round-trip");
    out << "JSON round-trip OK, clickAction preserved" << Qt::endl;

    // ── 4. deleteProfile ─────────────────────────────────────────────────────
    CHECK(pm.deleteProfile(newId), "deleteProfile failed");
    CHECK(pm.getProfile(newId) == nullptr, "profile still in memory after delete");
    CHECK(!QFile::exists(expectedFile), "profile file still on disk after delete");
    out << "Deleted profile, file removed" << Qt::endl;

    // ── 5. cannot delete system/default profiles ─────────────────────────────
    // System profiles (loaded from ./profiles/) have isDefault=true or are
    // not present in the user profile dir — deleteProfile() should refuse them.
    const QStringList remainingIds = pm.getProfileIds();
    out << "Loaded " << remainingIds.size() << " system profile(s)" << Qt::endl;
    for (const QString &id : remainingIds) {
        const Profile *sp = pm.getProfile(id);
        if (sp && sp->isDefault) {
            bool deleted = pm.deleteProfile(id);
            CHECK(!deleted, "should not be able to delete default/system profile: " + id);
            out << "Correctly refused to delete system profile: " << id << Qt::endl;
        }
    }

    out << "PASS" << Qt::endl;

    // Cleanup
    QDir(tmpDir).removeRecursively();
    return 0;
}
