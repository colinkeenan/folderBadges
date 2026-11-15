#include "settings.h"

#include <QSettings>
#include <QStandardPaths>

namespace FolderBadges
{

Settings Settings::load()
{
    Settings s;
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/folderbadges/folderbadges.conf");
    const QSettings conf(path, QSettings::IniFormat);
    s.maxDepth = conf.value(QStringLiteral("maxDepth"), s.maxDepth).toInt();
    s.maxEntries = conf.value(QStringLiteral("maxEntries"), s.maxEntries).toInt();
    s.minShare = conf.value(QStringLiteral("minShare"), s.minShare).toDouble();
    s.sameTypeShare = conf.value(QStringLiteral("sameTypeShare"), s.sameTypeShare).toDouble();
    s.sameTypeMinFiles = conf.value(QStringLiteral("sameTypeMinFiles"), s.sameTypeMinFiles).toInt();
    s.maxAgeHours = conf.value(QStringLiteral("maxAgeHours"), s.maxAgeHours).toInt();
    s.ignoredDirs = conf.value(QStringLiteral("ignoredDirs"), s.ignoredDirs).toStringList();
    s.excludedPaths = conf.value(QStringLiteral("excludedPaths"), s.excludedPaths).toStringList();
    return s;
}

bool Settings::isExcluded(const QString &path) const
{
    for (const QString &prefix : excludedPaths) {
        if (path == prefix || path.startsWith(prefix + QLatin1Char('/'))) {
            return true;
        }
    }
    return false;
}

}
