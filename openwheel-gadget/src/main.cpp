// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QDebug>

#ifndef NO_KDE_FRAMEWORKS
#include <KAboutData>
#include <KLocalizedString>
#include <KDBusService>
#else
#define i18n(x) QString(x)
#endif

#include "core/dialcontroller.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application properties
    QApplication::setApplicationName(QStringLiteral("openwheel-gadget"));
    QApplication::setApplicationDisplayName(i18n("OpenWheel Gadget"));
    QApplication::setOrganizationDomain(QStringLiteral("openwheel.org"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

#ifndef NO_KDE_FRAMEWORKS
    // Set KDE-specific application metadata
    KAboutData aboutData(
        QStringLiteral("openwheel-gadget"),
        i18n("OpenWheel Gadget"),
        QStringLiteral("0.1.0"),
        i18n("Creative dial controller for KDE Plasma"),
        KAboutLicense::GPL_V3,
        i18n("© 2025 OpenWheel Contributors")
    );

    aboutData.addAuthor(
        i18n("OpenWheel Contributors"),
        i18n("Developer"),
        QStringLiteral("[email protected]"),
        QStringLiteral("https://github.com/fredaime/openwheel")
    );

    KAboutData::setApplicationData(aboutData);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("input-dial")));

    // Ensure single instance
    KDBusService service(KDBusService::Unique);
#else
    qDebug() << "Running without KDE Frameworks (limited features)";
#endif

    // Set QML style
#ifndef NO_KDE_FRAMEWORKS
    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
#else
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
#endif

    // Create the dial controller
    DialController controller;
    controller.initialize();

    qDebug() << "============================================";
    qDebug() << "OpenWheel Gadget v0.1.0";
    qDebug() << "============================================";
    qDebug() << "Status: Listening for dial events...";
    qDebug() << "Current profile:" << controller.currentProfileName();
    qDebug() << "";
    qDebug() << "Rotate the dial to trigger actions.";
    qDebug() << "Press the dial button to show/hide overlay.";
    qDebug() << "";
    qDebug() << "Press Ctrl+C to quit.";
    qDebug() << "============================================";

    return app.exec();
}
