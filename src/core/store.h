#pragma once

#include <QString>

#include <optional>

namespace FolderBadges
{

/**
 * A folder's stored classification, kept in the `user.folderbadges` extended attribute of
 * the folder itself so that any frontend (or a shell script) can read it:
 *
 *     1;<type id or "none">;<unix time of classification>;<auto|pinned>
 */
struct StoredBadge {
    QString type; // empty = classified, no badge
    qint64 stamp = 0;
    bool pinned = false;
};

namespace Store
{
inline constexpr const char *AttributeName = "user.folderbadges";

std::optional<StoredBadge> read(const QString &directory);
/** Returns false if the attribute could not be written (read-only folder, no xattr support). */
bool write(const QString &directory, const StoredBadge &badge);
bool clear(const QString &directory);
}

}
