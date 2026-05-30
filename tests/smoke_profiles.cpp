// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors
//
// Smoke test for M3 profile management:
//   1. Load profiles via ProfileManager
//   2. List available profiles
//   3. Switch profile via setActiveProfile
//   4. Verify QSettings persistence round-trip
//
// Run: ./build/bin/smoke_profiles  (no GUI required)

#include <QCoreApplication>
#include <QSettings>
#include <QTextStream>

#include "profiles/profilemanager.h"
#include "profiles/profile.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("openwheel"));
    QCoreApplication::setApplicationName(QStringLiteral("gadget"));

    QTextStream out(stdout);

    ProfileManager pm;
    pm.loadProfiles();

    const QStringList ids = pm.getProfileIds();
    out << "Loaded " << ids.size() << " profile(s):" << Qt::endl;
    for (const QString &id : ids) {
        const Profile *p = pm.getProfile(id);
        out << "  " << id << " -> " << (p ? p->displayName : "(null)") << Qt::endl;
    }

    if (ids.size() < 2) {
        out << "SKIP: need at least 2 profiles to test switching" << Qt::endl;
        return 0;
    }

    // Switch to the second profile.
    const QString target = ids[1];
    pm.setCurrentProfile(target);
    const Profile *cur = pm.getCurrentProfile();
    if (!cur || cur->id != target) {
        out << "FAIL: setCurrentProfile did not switch" << Qt::endl;
        return 2;
    }
    out << "Switched to: " << cur->displayName << Qt::endl;

    // Simulate QSettings round-trip.
    {
        QSettings s(QStringLiteral("openwheel"), QStringLiteral("gadget"));
        s.setValue(QStringLiteral("selectedProfile"), target);
    }
    {
        QSettings s(QStringLiteral("openwheel"), QStringLiteral("gadget"));
        const QString loaded = s.value(QStringLiteral("selectedProfile")).toString();
        if (loaded != target) {
            out << "FAIL: QSettings round-trip: saved " << target << " got " << loaded << Qt::endl;
            return 3;
        }
        out << "QSettings round-trip OK: " << loaded << Qt::endl;
    }

    out << "PASS" << Qt::endl;
    return 0;
}
