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
 * 2. Almost all files directly in the folder have one type.
 * 3. The same with each subfolder counted as one more item of its own (recursively
 *    determined) type.
 * 4. Otherwise the dominant file type of a bounded breadth-first scan.
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
    Classification classifyAt(const QString &directory, int level, int &budget) const;

    TypeTable m_table;
    Settings m_settings;
};

}
