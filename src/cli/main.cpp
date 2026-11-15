#include "classifier.h"
#include "store.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

using namespace FolderBadges;

static QTextStream out(stdout);
static QTextStream err(stderr);

static int usage()
{
    err << "Usage:\n"
           "  folderbadges classify DIR...   show what the classifier decides (changes nothing)\n"
           "  folderbadges get DIR...        show the badge stored on a folder\n"
           "  folderbadges set DIR TYPE      pin a badge ('none' pins \"no badge\")\n"
           "  folderbadges clear DIR...      forget the stored badge, back to automatic\n"
           "  folderbadges types             list known badge types\n";
    return 2;
}

// Tell a running (or D-Bus activatable) daemon, and through it open file managers.
static void notifyDaemon(const QString &path)
{
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("io.github.folderbadges"),
                                                       QStringLiteral("/FolderBadges"),
                                                       QStringLiteral("io.github.folderbadges.Classifier"),
                                                       QStringLiteral("Refresh"));
    call << path;
    QDBusConnection::sessionBus().call(call, QDBus::Block, 2000);
}

static QString absolute(const QString &arg)
{
    return QDir::cleanPath(QFileInfo(arg).absoluteFilePath());
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments().mid(1);
    if (args.isEmpty()) {
        return usage();
    }
    const QString command = args.first();
    const QStringList rest = args.mid(1);
    const TypeTable table = TypeTable::load();

    if (command == QLatin1String("types") && rest.isEmpty()) {
        for (const BadgeType &t : table.types()) {
            out << t.id.leftJustified(12) << t.name.leftJustified(16) << t.icons.join(QLatin1String(", ")) << '\n';
        }
        return 0;
    }

    if (rest.isEmpty()) {
        return usage();
    }
    // `classify` and `get` are handy with shell globs, so they skip non-folders instead of failing.
    const bool lenient = command == QLatin1String("classify") || command == QLatin1String("get");
    QStringList dirs;
    for (const QString &arg : command == QLatin1String("set") ? rest.mid(0, 1) : rest) {
        if (QFileInfo(arg).isDir()) {
            dirs << arg;
        } else if (!lenient) {
            err << arg << ": not a directory\n";
            return 1;
        }
    }

    if (command == QLatin1String("classify")) {
        const Classifier classifier(table, Settings::load());
        for (const QString &arg : std::as_const(dirs)) {
            const Classification c = classifier.classify(absolute(arg));
            out << arg << ": " << (c.type.isEmpty() ? QStringLiteral("none") : c.type) << " (" << c.reason << ")\n";
        }
        return 0;
    }

    if (command == QLatin1String("get")) {
        for (const QString &arg : std::as_const(dirs)) {
            const auto stored = Store::read(absolute(arg));
            out << arg << ": ";
            if (!stored) {
                out << "not classified yet\n";
            } else {
                out << (stored->type.isEmpty() ? QStringLiteral("none") : stored->type) << (stored->pinned ? " (pinned)" : " (automatic)") << '\n';
            }
        }
        return 0;
    }

    if (command == QLatin1String("set") && rest.size() == 2) {
        const QString path = absolute(rest[0]);
        const QString type = rest[1] == QLatin1String("none") ? QString() : rest[1];
        if (!type.isEmpty() && !table.find(type)) {
            err << "unknown type '" << type << "', see `folderbadges types`\n";
            return 1;
        }
        if (!Store::write(path, StoredBadge{type, QDateTime::currentSecsSinceEpoch(), true})) {
            err << path << ": cannot write the " << Store::AttributeName << " extended attribute\n";
            return 1;
        }
        notifyDaemon(path);
        return 0;
    }

    if (command == QLatin1String("clear")) {
        int status = 0;
        for (const QString &arg : std::as_const(dirs)) {
            const QString path = absolute(arg);
            if (!Store::clear(path)) {
                err << path << ": cannot remove the " << Store::AttributeName << " extended attribute\n";
                status = 1;
                continue;
            }
            notifyDaemon(path);
        }
        return status;
    }

    return usage();
}
