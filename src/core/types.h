#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace FolderBadges
{

struct BadgeType {
    QString id;
    QString name;
    QStringList icons; // theme icon names, in order of preference
    QStringList markers; // see data/types.json
    QStringList dirNames;
    QStringList extensions; // lower case, without dot
    QStringList shared; // lower case, without dot
};

class TypeTable
{
public:
    /** Built-in table merged with ~/.config/folderbadges/types.json (if present). */
    static TypeTable load();
    /** Only the table compiled into the binary (data/types.json). */
    static TypeTable builtin();
    /** Parse a table from JSON; used by load() and by tests. */
    static TypeTable fromJson(const QByteArray &json, QString *error = nullptr);

    /** User entries replace built-in ones with the same id; new ids are tried first. */
    void merge(const TypeTable &user);

    const QList<BadgeType> &types() const
    {
        return m_types;
    }
    const BadgeType *find(const QString &id) const;
    QStringList iconsFor(const QString &id) const;

private:
    QList<BadgeType> m_types;
};

}
