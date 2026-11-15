#include "service.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("folderbadgesd"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("FolderBadges classifier daemon"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption persist(QStringLiteral("persist"), QStringLiteral("Do not exit after being idle for ten minutes."));
    parser.addOption(persist);
    parser.process(app);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical("folderbadgesd: no D-Bus session bus");
        return 1;
    }

    Service service(parser.isSet(persist));
    bus.registerObject(QStringLiteral("/FolderBadges"), &service, QDBusConnection::ExportScriptableContents);
    if (!bus.registerService(QStringLiteral("io.github.folderbadges"))) {
        qCritical("folderbadgesd: io.github.folderbadges is already owned, is another instance running?");
        return 1;
    }
    return app.exec();
}
