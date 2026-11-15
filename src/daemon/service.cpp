#include "service.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

using namespace FolderBadges;

static constexpr int IdleExitMs = 10 * 60 * 1000;

Service::Service(bool persist, QObject *parent)
    : QObject(parent)
    , m_classifier(TypeTable::load(), Settings::load())
{
    // Scanning is I/O bound background work; never compete with the file manager for it.
    m_pool.setMaxThreadCount(2);
    m_pool.setThreadPriority(QThread::LowPriority);

    if (!persist) {
        // Started on demand through D-Bus activation, so there is no reason to linger.
        m_idleTimer.setInterval(IdleExitMs);
        connect(&m_idleTimer, &QTimer::timeout, this, [this] {
            if (m_pending.isEmpty()) {
                QCoreApplication::quit();
            }
        });
        m_idleTimer.start();
    }
}

void Service::touch()
{
    if (m_idleTimer.isActive()) {
        m_idleTimer.start();
    }
}

QString Service::Version() const
{
    return QCoreApplication::applicationVersion();
}

std::optional<StoredBadge> Service::lookup(const QString &path) const
{
    if (auto stored = Store::read(path)) {
        return stored;
    }
    if (const auto it = m_memory.constFind(path); it != m_memory.constEnd()) {
        return *it;
    }
    return std::nullopt;
}

bool Service::isFresh(const QString &path, const StoredBadge &badge) const
{
    if (badge.pinned) {
        return true;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - badge.stamp > qint64(m_classifier.settings().maxAgeHours) * 3600) {
        return false;
    }
    // Entries were added to or removed from the folder itself since we last looked.
    return QFileInfo(path).lastModified().toSecsSinceEpoch() <= badge.stamp;
}

QString Service::Query(const QString &rawPath, QStringList &icons)
{
    touch();
    const QString path = QDir::cleanPath(rawPath);
    if (!QDir::isAbsolutePath(path) || m_classifier.settings().isExcluded(path) || !QFileInfo(path).isDir()) {
        return QString();
    }

    if (const auto stored = lookup(path); stored && isFresh(path, *stored)) {
        icons = m_classifier.table().iconsFor(stored->type);
        return stored->type;
    }

    if (calledFromDBus()) {
        setDelayedReply(true);
        m_pending[path] << message();
    }
    classifyAsync(path, false);
    return QString();
}

void Service::Refresh(const QString &rawPath)
{
    touch();
    const QString path = QDir::cleanPath(rawPath);
    if (!QDir::isAbsolutePath(path) || !QFileInfo(path).isDir()) {
        return;
    }
    m_memory.remove(path);
    if (const auto stored = Store::read(path); stored && stored->pinned) {
        Q_EMIT BadgeChanged(path, stored->type, m_classifier.table().iconsFor(stored->type));
        return;
    }
    if (!m_classifier.settings().isExcluded(path)) {
        classifyAsync(path, true);
    }
}

void Service::classifyAsync(const QString &path, bool announce)
{
    const bool running = m_pendingAnnounce.contains(path);
    m_pendingAnnounce[path] = m_pendingAnnounce.value(path) || announce;
    if (running) {
        return;
    }
    m_pool.start([this, path] {
        const Classification result = m_classifier.classify(path);
        QMetaObject::invokeMethod(
            this,
            [this, path, type = result.type] {
                finished(path, type, m_pendingAnnounce.take(path));
            },
            Qt::QueuedConnection);
    });
}

void Service::finished(const QString &path, const QString &classifiedType, bool announce)
{
    const auto previous = lookup(path);
    QString type = classifiedType;
    if (previous && previous->pinned) {
        // Pinned while we were scanning: the user's choice wins.
        type = previous->type;
    } else {
        const StoredBadge badge{type, QDateTime::currentSecsSinceEpoch(), false};
        if (Store::write(path, badge)) {
            m_memory.remove(path);
        } else {
            m_memory.insert(path, badge);
        }
    }

    const QStringList icons = m_classifier.table().iconsFor(type);
    const QList<QDBusMessage> waiting = m_pending.take(path);
    for (const QDBusMessage &call : waiting) {
        QDBusConnection::sessionBus().send(call.createReply({type, icons}));
    }
    if (announce || (previous && previous->type != type)) {
        Q_EMIT BadgeChanged(path, type, icons);
    }
    touch();
}
