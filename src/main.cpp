#include "Pet.h"
#include "Tray.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    // QApplication (not QGuiApplication) for the native tray menu.
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Kit"));
    QApplication::setOrganizationName(QStringLiteral("Kit"));
    QApplication::setOrganizationDomain(QStringLiteral("kit.app"));
    QApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));
    QApplication::setQuitOnLastWindowClosed(false);

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QLockFile instanceLock(dataDir + QStringLiteral("/instance.lock"));
    if (!instanceLock.tryLock(100)) {
        qWarning() << "Kit is already running";
        return 0;
    }

    Pet pet;
    Tray tray(&pet);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({ { QStringLiteral("pet"), QVariant::fromValue(&pet) } });
    engine.loadFromModule("Kit", "Main");

    auto* window = engine.rootObjects().isEmpty()
        ? nullptr
        : qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (!window) {
        qCritical() << "Kit: failed to load the QML window";
        return 1;
    }
    pet.attach(window);
    return app.exec();
}
