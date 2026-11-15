/*
 * Dolphin / KIO overlay icon plugin for FolderBadges.
 *
 * This is deliberately thin: it asks folderbadgesd (over D-Bus, asynchronously) which badge
 * a folder has and returns the name of a theme icon to paint on top of the folder icon.
 * Because only icon *names* are returned, both the folder and the badge are drawn by the
 * current icon theme and follow theme changes.
 */

#include <KIconLoader>
#include <KOverlayIconPlugin>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QSet>
#include <QUrl>

static const QString s_service = QStringLiteral("io.github.folderbadges");
static const QString s_object = QStringLiteral("/FolderBadges");
static const QString s_interface = QStringLiteral("io.github.folderbadges.Classifier");

static constexpr qint64 CacheSeconds = 5 * 60;
static constexpr int CacheLimit = 20000;

class FolderBadgesPlugin : public KOverlayIconPlugin
{
    Q_PLUGIN_METADATA(IID "org.kde.overlayicon.folderbadges")
    Q_OBJECT

public:
    FolderBadgesPlugin()
    {
        QDBusConnection::sessionBus()
            .connect(s_service, s_object, s_interface, QStringLiteral("BadgeChanged"), this, SLOT(badgeChanged(QString, QString, QStringList)));
    }

    // Called on the GUI thread for every item in a view, so it only ever looks at the cache.
    QStringList getOverlays(const QUrl &url) override
    {
        if (!url.isLocalFile()) {
            return {};
        }
        const QString path = QDir::cleanPath(url.toLocalFile());
        const auto it = m_cache.constFind(path);
        if (it != m_cache.constEnd()) {
            if (QDateTime::currentSecsSinceEpoch() - it->fetched > CacheSeconds) {
                request(path, url);
            }
            return overlaysFor(it->icons);
        }
        if (QFileInfo(path).isDir()) {
            request(path, url);
        }
        return {};
    }

private Q_SLOTS:
    void badgeChanged(const QString &path, const QString &type, const QStringList &icons)
    {
        Q_UNUSED(type)
        if (m_cache.contains(path)) {
            store(path, QUrl::fromLocalFile(path), icons);
        }
    }

private:
    struct Entry {
        QStringList icons; // candidates from the daemon, best first
        qint64 fetched = 0;
    };

    void request(const QString &path, const QUrl &url)
    {
        if (m_pending.contains(path)) {
            return;
        }
        m_pending.insert(path);
        QDBusMessage call = QDBusMessage::createMethodCall(s_service, s_object, s_interface, QStringLiteral("Query"));
        call << path;
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, path, url](QDBusPendingCallWatcher *self) {
            self->deleteLater();
            m_pending.remove(path);
            const QDBusPendingReply<QString, QStringList> reply = *self;
            // On error (daemon not installed, ...) remember "no badge" so we do not ask again
            // for every repaint; the entry expires like any other.
            store(path, url, reply.isError() ? QStringList() : reply.argumentAt<1>());
        });
    }

    void store(const QString &path, const QUrl &url, const QStringList &icons)
    {
        if (m_cache.size() > CacheLimit) {
            m_cache.clear();
        }
        const QStringList before = overlaysFor(m_cache.value(path).icons);
        m_cache.insert(path, Entry{icons, QDateTime::currentSecsSinceEpoch()});
        const QStringList after = overlaysFor(icons);
        if (before != after) {
            Q_EMIT overlaysChanged(url, after);
        }
    }

    // First candidate the *current* icon theme actually has.
    QStringList overlaysFor(const QStringList &candidates)
    {
        if (candidates.isEmpty()) {
            return {};
        }
        if (const QString theme = QIcon::themeName(); theme != m_theme) {
            m_theme = theme;
            m_resolved.clear();
        }
        const QString key = candidates.join(QLatin1Char(';'));
        auto it = m_resolved.constFind(key);
        if (it == m_resolved.constEnd()) {
            QString found;
            for (const QString &name : candidates) {
                if (KIconLoader::global()->hasIcon(name)) {
                    found = name;
                    break;
                }
            }
            it = m_resolved.insert(key, found);
        }
        return it->isEmpty() ? QStringList() : QStringList{*it};
    }

    QHash<QString, Entry> m_cache;
    QSet<QString> m_pending;
    QHash<QString, QString> m_resolved;
    QString m_theme;
};

#include "folderbadgesplugin.moc"
