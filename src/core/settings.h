#pragma once

#include <QStringList>

namespace FolderBadges
{

/** Tunables, read from ~/.config/folderbadges/folderbadges.conf (INI, all keys optional). */
struct Settings {
    int maxDepth = 4; // how many levels below the folder are scanned
    int maxEntries = 5000; // directory entries looked at per folder before giving up
    double minShare = 0.4; // dominant type must make up this fraction of all files seen
    int sameTypeMinFiles = 2; // this many files of one type (and no others) directly in a folder decide it
    int maxAgeHours = 24; // automatic results are re-checked after this long
    QStringList ignoredDirs = {
        QStringLiteral("node_modules"), QStringLiteral("target"),      QStringLiteral("build"), QStringLiteral("dist"),
        QStringLiteral("vendor"),       QStringLiteral("__pycache__"), QStringLiteral("venv"),  QStringLiteral("third_party"),
    };
    QStringList excludedPaths = {QStringLiteral("/proc"), QStringLiteral("/sys"), QStringLiteral("/dev")};

    static Settings load();
    bool isExcluded(const QString &path) const;
};

}
