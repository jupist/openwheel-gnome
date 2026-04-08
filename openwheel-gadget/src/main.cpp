// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 OpenWheel Contributors

#include <QApplication>
#include <QLockFile>
#include <QStandardPaths>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickImageProvider>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QDebug>

// ---------------------------------------------------------------------------
// IconImageProvider — serves QIcon::fromTheme() images to QML via
// the "image://icon/<freedesktop-name>" URI scheme.  Works on both GNOME
// and KDE because QIcon::fromTheme() honours XDG_DATA_DIRS.
// ---------------------------------------------------------------------------
class IconImageProvider : public QQuickImageProvider
{
public:
    IconImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        const int dim = (requestedSize.width() > 0) ? requestedSize.width() : 32;
        QIcon icon = QIcon::fromTheme(id);
        if (icon.isNull()) {
            // Return a transparent placeholder so the Image item stays hidden
            // (its `visible` property is gated on status === Image.Ready).
            QPixmap px(dim, dim);
            px.fill(Qt::transparent);
            if (size) *size = px.size();
            return px;
        }
        QPixmap px = icon.pixmap(dim, dim);
        if (size) *size = px.size();
        return px;
    }
};

#ifdef HAVE_KF6
#include <KAboutData>
#include <KLocalizedString>
#include <KDBusService>
#else
// Minimal i18n shim so code compiles without KDE Frameworks.
#define i18n(x) QString(x)
#endif

#include "core/dialcontroller.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Prevent multiple simultaneous instances — two instances produce double overlays.
    const QString lockPath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
                             + QStringLiteral("/openwheel-gadget.lock");
    QLockFile instanceLock(lockPath);
    if (!instanceLock.tryLock(100)) {
        qWarning() << "openwheel-gadget: another instance is already running — exiting.";
        return 0;
    }

    QApplication::setApplicationName(QStringLiteral("openwheel-gadget"));
    QApplication::setApplicationDisplayName(QStringLiteral("OpenWheel Gadget"));
    QApplication::setOrganizationName(QStringLiteral("openwheel"));
    QApplication::setOrganizationDomain(QStringLiteral("openwheel.org"));
    QApplication::setApplicationVersion(QStringLiteral(OPENWHEEL_VERSION));

#ifdef HAVE_KF6
    KAboutData aboutData(
        QStringLiteral("openwheel-gadget"),
        i18n("OpenWheel Gadget"),
        QStringLiteral(OPENWHEEL_VERSION),
        i18n("Creative dial controller"),
        KAboutLicense::GPL_V3,
        i18n("© 2025 OpenWheel Contributors")
    );
    aboutData.addAuthor(
        i18n("OpenWheel Contributors"),
        i18n("Developer"),
        QStringLiteral(""),
        QStringLiteral("https://github.com/jupist/openwheel-gnome")
    );
    KAboutData::setApplicationData(aboutData);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("input-dial")));

    // Enforce single-instance on KDE.
    KDBusService service(KDBusService::Unique);
#endif

    // Use the Fusion style everywhere — it has no KDE/GNOME-specific deps
    // and renders consistently on both desktops. Kirigami is no longer
    // imported in the QML (colours switched to plain Qt palette queries).
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    DialController controller;
    controller.initialize();

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new IconImageProvider());
    engine.rootContext()->setContextProperty(QStringLiteral("dialController"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/ui/OverlayWindow.qml")));

    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Failed to load overlay QML";
        return 1;
    }

    // Settings window is declared inside OverlayWindow.qml as a persistent
    // child — dialController.settingsRequested() is handled entirely in QML.

    // ── System tray icon ─────────────────────────────────────────────────────
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        auto *trayIcon = new QSystemTrayIcon(&app);
        trayIcon->setIcon(QIcon::fromTheme(QStringLiteral("openwheel"),
                                           QIcon::fromTheme(QStringLiteral("input-dial"))));
        trayIcon->setToolTip(QStringLiteral("OpenWheel Gadget"));

        auto *trayMenu = new QMenu();
        auto *settingsAction = trayMenu->addAction(QStringLiteral("Settings…"));
        QObject::connect(settingsAction, &QAction::triggered, &controller, &DialController::openSettings);
        trayMenu->addSeparator();
        auto *quitAction = trayMenu->addAction(QStringLiteral("Quit"));
        QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);

        trayIcon->setContextMenu(trayMenu);
        trayIcon->show();

        // Left-click also opens settings
        QObject::connect(trayIcon, &QSystemTrayIcon::activated,
                         [&controller](QSystemTrayIcon::ActivationReason reason) {
                             if (reason == QSystemTrayIcon::Trigger) {
                                 controller.openSettings();
                             }
                         });
    } else {
        qDebug() << "System tray not available — use dialController.openSettings() from QML";
    }

    qDebug() << "============================================";
    qDebug() << "OpenWheel Gadget v" OPENWHEEL_VERSION;
    qDebug() << "============================================";
    qDebug() << "Profile:" << controller.currentProfileName();
    qDebug() << "Long-press the dial button to pick a profile.";
    qDebug() << "Double-click to cycle functions.";
    qDebug() << "============================================";

    return app.exec();
}
