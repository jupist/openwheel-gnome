// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors
//
// Standalone smoke test for the M2 brightness backend chain. Constructs
// an ActionExecutor and fires a brightness-up keypress action, then a
// brightness-down. Reads the actual /sys/class/backlight value before
// and after to confirm the call landed.
//
// Not part of ctest by default — it touches real hardware. Run manually:
//   ./build/bin/smoke_brightness
// Exits 0 on success, non-zero on failure.

#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QTimer>

#include "core/actionexecutor.h"
#include "profiles/profile.h"

static int readBacklight()
{
    QDir d(QStringLiteral("/sys/class/backlight"));
    const auto entries = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) return -1;
    QFile f(QStringLiteral("/sys/class/backlight/") + entries.first() + QStringLiteral("/brightness"));
    if (!f.open(QIODevice::ReadOnly)) return -1;
    return QString::fromLatin1(f.readAll()).trimmed().toInt();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    int before = readBacklight();
    out << "Backlight before: " << before << Qt::endl;
    if (before < 0) {
        out << "FAIL: no backlight device found" << Qt::endl;
        return 1;
    }

    ActionExecutor exec;

    ActionConfig up;
    up.type = ActionConfig::Type::Keyboard;
    up.keys = QStringLiteral("XF86MonBrightnessUp");

    ActionConfig down;
    down.type = ActionConfig::Type::Keyboard;
    down.keys = QStringLiteral("XF86MonBrightnessDown");

    out << "--> brightness UP" << Qt::endl;
    exec.executeAction(up, 1);
    int afterUp = readBacklight();
    out << "Backlight after up: " << afterUp << Qt::endl;

    out << "--> brightness DOWN" << Qt::endl;
    exec.executeAction(down, 1);
    int afterDown = readBacklight();
    out << "Backlight after down: " << afterDown << Qt::endl;

    bool ok = (afterUp > before) && (afterDown < afterUp);
    out << (ok ? "PASS" : "FAIL") << Qt::endl;
    return ok ? 0 : 2;
}
