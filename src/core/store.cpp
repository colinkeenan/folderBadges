#include "store.h"

#include <QByteArray>
#include <QFile>
#include <QList>

#include <cerrno>
#include <sys/xattr.h>

namespace FolderBadges::Store
{

std::optional<StoredBadge> read(const QString &directory)
{
    char buffer[256];
    const ssize_t size = ::getxattr(QFile::encodeName(directory).constData(), AttributeName, buffer, sizeof(buffer));
    if (size <= 0) {
        return std::nullopt;
    }
    const QList<QByteArray> fields = QByteArray(buffer, size).split(';');
    if (fields.size() < 4 || fields[0] != "1") {
        return std::nullopt;
    }
    StoredBadge badge;
    badge.type = fields[1] == "none" ? QString() : QString::fromUtf8(fields[1]);
    badge.stamp = fields[2].toLongLong();
    badge.pinned = fields[3] == "pinned";
    return badge;
}

bool write(const QString &directory, const StoredBadge &badge)
{
    const QByteArray value = "1;" + (badge.type.isEmpty() ? QByteArray("none") : badge.type.toUtf8()) + ';' + QByteArray::number(badge.stamp) + ';'
        + (badge.pinned ? "pinned" : "auto");
    return ::setxattr(QFile::encodeName(directory).constData(), AttributeName, value.constData(), value.size(), 0) == 0;
}

bool clear(const QString &directory)
{
    return ::removexattr(QFile::encodeName(directory).constData(), AttributeName) == 0 || errno == ENODATA;
}

}
