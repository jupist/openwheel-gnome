// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include "applicationmatcher.h"

#ifndef NO_KDE_FRAMEWORKS
#include <KWindowSystem>
#include <KWindowInfo>
#ifdef HAVE_X11
#include <KX11Extras>
#endif
#endif

#include <QFile>
#include <QDebug>

ApplicationMatcher::ApplicationMatcher(QObject *parent)
    : QObject(parent)
{
}

ApplicationMatcher::~ApplicationMatcher()
{
    stopMonitoring();
}

void ApplicationMatcher::startMonitoring()
{
    if (m_monitoring) {
        return;
    }

    qDebug() << "Starting window monitoring";

#ifndef NO_KDE_FRAMEWORKS
    // Connect to window system signals
#ifdef HAVE_X11
    if (KWindowSystem::isPlatformX11()) {
        connect(KX11Extras::self(), &KX11Extras::activeWindowChanged,
                this, &ApplicationMatcher::onActiveWindowChanged);
        qDebug() << "Using X11 window tracking";
    }
#endif
#else
    qDebug() << "Window monitoring not available without KDE Frameworks";
    qDebug() << "Using default profile only";
#endif

    m_monitoring = true;

#ifndef NO_KDE_FRAMEWORKS
    // Get initial window
    updateCurrentWindow();
#endif
}

void ApplicationMatcher::stopMonitoring()
{
    if (!m_monitoring) {
        return;
    }

    qDebug() << "Stopping window monitoring";

#ifndef NO_KDE_FRAMEWORKS
#ifdef HAVE_X11
    if (KWindowSystem::isPlatformX11()) {
        disconnect(KX11Extras::self(), &KX11Extras::activeWindowChanged,
                   this, &ApplicationMatcher::onActiveWindowChanged);
    }
#endif
#endif

    m_monitoring = false;
}

void ApplicationMatcher::onActiveWindowChanged()
{
#ifndef NO_KDE_FRAMEWORKS
    updateCurrentWindow();
#endif
}

void ApplicationMatcher::updateCurrentWindow()
{
#ifndef NO_KDE_FRAMEWORKS
    WId windowId = 0;
#ifdef HAVE_X11
    if (KWindowSystem::isPlatformX11()) {
        windowId = KX11Extras::activeWindow();
    }
#endif

    if (windowId == 0) {
        qDebug() << "No active window";
        return;
    }

    KWindowInfo info(windowId, NET::WMName | NET::WMVisibleName, NET::WM2WindowClass);

    QString windowClass;
    QString windowName;
    QString processName;

    // Get window class
#ifdef HAVE_X11
    if (KWindowSystem::isPlatformX11()) {
        windowClass = info.windowClassClass();
    } else
#endif
    {
        // On Wayland, window class might not be available
        windowClass = info.windowClassName();
    }

    // Get window name
    windowName = info.name();
    if (windowName.isEmpty()) {
        windowName = info.visibleName();
    }

    // Get process name from PID
    int pid = info.pid();
    if (pid > 0) {
        processName = getProcessName(pid);
    }

    // Only emit if something changed
    if (windowClass != m_currentWindowClass ||
        windowName != m_currentWindowName ||
        processName != m_currentProcessName) {

        m_currentWindowClass = windowClass;
        m_currentWindowName = windowName;
        m_currentProcessName = processName;

        qDebug() << "Active window changed:";
        qDebug() << "  Class:" << windowClass;
        qDebug() << "  Name:" << windowName;
        qDebug() << "  Process:" << processName;

        Q_EMIT windowChanged(windowClass, windowName, processName);
    }
#endif
}

QString ApplicationMatcher::getProcessName(int pid) const
{
    // Try to read process name from /proc
    QString commPath = QStringLiteral("/proc/%1/comm").arg(pid);
    QFile commFile(commPath);

    if (commFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString comm = QString::fromUtf8(commFile.readAll()).trimmed();
        if (!comm.isEmpty()) {
            return comm;
        }
    }

    // Alternative: read from /proc/pid/cmdline
    QString cmdlinePath = QStringLiteral("/proc/%1/cmdline").arg(pid);
    QFile cmdlineFile(cmdlinePath);

    if (cmdlineFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray cmdline = cmdlineFile.readAll();
        // cmdline is null-separated, take the first part
        int nullPos = cmdline.indexOf('\0');
        if (nullPos > 0) {
            cmdline = cmdline.left(nullPos);
        }

        QString command = QString::fromUtf8(cmdline);
        // Extract just the executable name
        int lastSlash = command.lastIndexOf('/');
        if (lastSlash >= 0) {
            command = command.mid(lastSlash + 1);
        }

        if (!command.isEmpty()) {
            return command;
        }
    }

    return QString();
}
