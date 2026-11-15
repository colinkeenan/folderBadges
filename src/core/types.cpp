#include "types.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

// Resources in a static library are not registered automatically, and Q_INIT_RESOURCE
// has to be used outside of any namespace.
static void initResources()
{
    Q_INIT_RESOURCE(folderbadges);
}

namespace FolderBadges
{

static QStringList stringList(const QJsonValue &value, bool lower = false)
{
    QStringList out;
    const QJsonArray array = value.toArray();
    for (const QJsonValue &v : array) {
        const QString s = v.toString();
        if (!s.isEmpty()) {
            out << (lower ? s.toLower() : s);
        }
    }
    return out;
}

TypeTable TypeTable::fromJson(const QByteArray &json, QString *error)
{
    TypeTable table;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) {
            *error = parseError.errorString();
        }
        return table;
    }
    const QJsonArray types = doc.object().value(QLatin1String("types")).toArray();
    for (const QJsonValue &value : types) {
        const QJsonObject o = value.toObject();
        BadgeType t;
        t.id = o.value(QLatin1String("id")).toString();
        // ';' is the field separator of the stored xattr value
        if (t.id.isEmpty() || t.id.contains(QLatin1Char(';')) || t.id == QLatin1String("none")) {
            continue;
        }
        t.name = o.value(QLatin1String("name")).toString(t.id);
        t.icons = stringList(o.value(QLatin1String("icons")));
        t.markers = stringList(o.value(QLatin1String("markers")));
        t.dirNames = stringList(o.value(QLatin1String("dirNames")));
        t.extensions = stringList(o.value(QLatin1String("extensions")), true);
        t.shared = stringList(o.value(QLatin1String("shared")), true);
        table.m_types << t;
    }
    return table;
}

TypeTable TypeTable::builtin()
{
    initResources();
    QFile file(QStringLiteral(":/folderbadges/types.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return fromJson(file.readAll());
}

TypeTable TypeTable::load()
{
    TypeTable table = builtin();

    const QString userPath = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, QStringLiteral("folderbadges/types.json"));
    if (!userPath.isEmpty()) {
        QFile user(userPath);
        if (user.open(QIODevice::ReadOnly)) {
            QString error;
            const TypeTable userTable = fromJson(user.readAll(), &error);
            if (!error.isEmpty()) {
                qWarning("folderbadges: ignoring %s: %s", qPrintable(userPath), qPrintable(error));
            } else {
                table.merge(userTable);
            }
        }
    }
    return table;
}

void TypeTable::merge(const TypeTable &user)
{
    QList<BadgeType> added;
    for (const BadgeType &t : user.m_types) {
        bool replaced = false;
        for (BadgeType &existing : m_types) {
            if (existing.id == t.id) {
                existing = t;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            added << t;
        }
    }
    m_types = added + m_types;
}

const BadgeType *TypeTable::find(const QString &id) const
{
    for (const BadgeType &t : m_types) {
        if (t.id == id) {
            return &t;
        }
    }
    return nullptr;
}

QStringList TypeTable::iconsFor(const QString &id) const
{
    const BadgeType *t = find(id);
    return t ? t->icons : QStringList();
}

}
