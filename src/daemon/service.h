#pragma once

#include "classifier.h"
#include "store.h"

#include <QDBusContext>
#include <QDBusMessage>
#include <QHash>
#include <QObject>
#include <QThreadPool>
#include <QTimer>

/**
 * D-Bus face of folderbadgesd.
 *
 *   service    io.github.folderbadges
 *   object     /FolderBadges
 *   interface  io.github.folderbadges.Classifier
 *
 * Frontends call Query() for a folder and listen to BadgeChanged(). They never classify
 * anything themselves and only need the returned icon names.
 */
class Service : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.folderbadges.Classifier")

public:
    explicit Service(bool persist, QObject *parent = nullptr);

public Q_SLOTS:
    /** Badge of a folder: type id ("" = none) and candidate theme icon names. Classifies if needed. */
    Q_SCRIPTABLE QString Query(const QString &path, QStringList &icons);
    /** Drop what is known about a folder, classify again (unless pinned) and emit BadgeChanged. */
    Q_SCRIPTABLE void Refresh(const QString &path);
    Q_SCRIPTABLE QString Version() const;

Q_SIGNALS:
    Q_SCRIPTABLE void BadgeChanged(const QString &path, const QString &type, const QStringList &icons);

private:
    std::optional<FolderBadges::StoredBadge> lookup(const QString &path) const;
    bool isFresh(const QString &path, const FolderBadges::StoredBadge &badge) const;
    void classifyAsync(const QString &path, bool announce);
    void finished(const QString &path, const QString &type, bool announce);
    void touch();

    FolderBadges::Classifier m_classifier;
    QThreadPool m_pool;
    // Results for folders whose xattr cannot be written.
    QHash<QString, FolderBadges::StoredBadge> m_memory;
    // Folder being classified -> callers waiting for the answer.
    QHash<QString, QList<QDBusMessage>> m_pending;
    QHash<QString, bool> m_pendingAnnounce;
    QTimer m_idleTimer;
};
