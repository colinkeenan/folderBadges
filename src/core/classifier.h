#pragma once

#include "settings.h"
#include "types.h"

namespace FolderBadges
{

struct Classification {
    QString type; // type id, empty = no badge
    QString reason; // human readable, for `folderbadges classify`
};

/**
 * Decides which badge a folder gets.
 *
 * 1. Marker files / folder name rules, in type table order (cheap, one directory listing).
 * 2. Otherwise the dominant file type of a bounded breadth-first scan.
 *
 * Thread safe: classify() only reads the table and settings.
 */
class Classifier
{
public:
    Classifier(TypeTable table, Settings settings);

    Classification classify(const QString &directory) const;

    const TypeTable &table() const
    {
        return m_table;
    }
    const Settings &settings() const
    {
        return m_settings;
    }

private:
    TypeTable m_table;
    Settings m_settings;
};

}
