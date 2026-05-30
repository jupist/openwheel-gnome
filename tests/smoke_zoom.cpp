// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors
//
// Standalone smoke test for the M2 zoom dispatch on non-KDE desktops.
// Constructs an ActionExecutor and fires a zoom-in keypress; this should
// flow through the daemon's InjectKey path as Ctrl+equal.
//
// Run manually after starting openwheel-daemon.

#include <QCoreApplication>
#include <QTextStream>

#include "core/actionexecutor.h"
#include "profiles/profile.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    ActionExecutor exec;

    ActionConfig zoomIn;
    zoomIn.type = ActionConfig::Type::Keyboard;
    zoomIn.keys = QStringLiteral("ZoomIn");

    ActionConfig zoomOut;
    zoomOut.type = ActionConfig::Type::Keyboard;
    zoomOut.keys = QStringLiteral("ZoomOut");

    out << "--> ZoomIn (expect Ctrl+equal via uinput)" << Qt::endl;
    exec.executeAction(zoomIn, 1);

    out << "--> ZoomOut (expect Ctrl+minus via uinput)" << Qt::endl;
    exec.executeAction(zoomOut, 1);

    out << "Done — check /dev/input/eventN for the OpenWheel Virtual Input device" << Qt::endl;
    return 0;
}
