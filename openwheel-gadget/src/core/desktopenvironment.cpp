// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors

#include "desktopenvironment.h"

#include <QString>
#include <qglobal.h>

namespace DesktopEnvironment {

QString sessionType()
{
    return qEnvironmentVariable("XDG_SESSION_TYPE");
}

bool isWayland()
{
    return sessionType().compare(QStringLiteral("wayland"), Qt::CaseInsensitive) == 0;
}

bool isX11()
{
    return sessionType().compare(QStringLiteral("x11"), Qt::CaseInsensitive) == 0;
}

QString currentDesktop()
{
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP");
}

bool isGnome()
{
    return currentDesktop().contains(QStringLiteral("GNOME"), Qt::CaseInsensitive);
}

bool isKde()
{
    const QString d = currentDesktop();
    return d.contains(QStringLiteral("KDE"), Qt::CaseInsensitive)
        || d.contains(QStringLiteral("Plasma"), Qt::CaseInsensitive);
}

} // namespace DesktopEnvironment
