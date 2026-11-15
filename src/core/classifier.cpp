#include "classifier.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <deque>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace FolderBadges
{

Classifier::Classifier(TypeTable table, Settings settings)
    : m_table(std::move(table))
    , m_settings(std::move(settings))
{
}

namespace
{

struct RootListing {
    QStringList files; // everything that is not a directory, for marker matching
    QStringList dirs;
    QStringList ownFiles; // visible regular files; symlinks are left out, as in the scan
    QStringList ownDirs; // visible subfolders, without symlinks and ignored names
};

bool anyMatches(const QString &glob, const QStringList &names)
{
    for (const QString &name : names) {
        if (QDir::match(glob, name)) {
            return true;
        }
    }
    return false;
}

// "a+b/" : every '+'-separated part must exist; a trailing '/' asks for a directory.
bool markerMatches(const QString &marker, const RootListing &root)
{
    const QStringList parts = marker.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return false;
    }
    for (const QString &part : parts) {
        const bool wantDir = part.endsWith(QLatin1Char('/'));
        const QString glob = wantDir ? part.chopped(1) : part;
        if (!anyMatches(glob, wantDir ? root.dirs : root.files)) {
            return false;
        }
    }
    return true;
}

}

Classification Classifier::classify(const QString &directory) const
{
    // Shared by the folder and every subfolder classified on its behalf, so that a huge
    // tree cannot turn one classification into thousands of scans.
    int budget = 4 * m_settings.maxEntries;
    return classifyAt(directory, 0, budget);
}

Classification Classifier::classifyAt(const QString &directory, int level, int &budget) const
{
    std::error_code ec;
    const fs::path rootPath = fs::path(directory.toStdString());
    if (!fs::is_directory(rootPath, ec)) {
        return {};
    }

    // ---- 1. marker files and folder name ----
    const QSet<QString> ignored(m_settings.ignoredDirs.begin(), m_settings.ignoredDirs.end());
    RootListing root;
    int rootEntries = 0;
    for (fs::directory_iterator it(rootPath, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
        if (++rootEntries > m_settings.maxEntries) {
            break;
        }
        --budget;
        std::error_code typeEc;
        const QString name = QString::fromStdString(it->path().filename().string());
        if (it->is_directory(typeEc)) {
            root.dirs << name;
            if (!name.startsWith(QLatin1Char('.')) && !it->is_symlink(typeEc) && !ignored.contains(name)) {
                root.ownDirs << name;
            }
        } else {
            root.files << name;
            if (!name.startsWith(QLatin1Char('.')) && !it->is_symlink(typeEc) && it->is_regular_file(typeEc)) {
                root.ownFiles << name;
            }
        }
    }

    // A marker listed by several types (CMakeLists.txt: C or C++) only narrows the choice
    // down to those types; the file scan below then picks between them.
    QList<int> contenders;
    QString sharedMarker;
    const QList<BadgeType> &types = m_table.types();
    const QString ownName = QFileInfo(directory).fileName();
    for (int i = 0; i < types.size() && contenders.isEmpty(); ++i) {
        for (const QString &glob : types[i].dirNames) {
            if (QDir::match(glob, ownName)) {
                return {types[i].id, QStringLiteral("folder name matches '%1'").arg(glob)};
            }
        }
        for (const QString &marker : types[i].markers) {
            if (!markerMatches(marker, root)) {
                continue;
            }
            for (int j = 0; j < types.size(); ++j) {
                if (types[j].markers.contains(marker)) {
                    contenders << j;
                }
            }
            if (contenders.size() == 1) {
                return {types[i].id, QStringLiteral("contains %1").arg(marker)};
            }
            sharedMarker = marker;
            break;
        }
    }

    // ---- 2. file types ----
    QHash<QString, int> extensionOwner; // extension -> index into types
    QHash<QString, QList<int>> sharedClaimants;
    for (int i = 0; i < types.size(); ++i) {
        for (const QString &ext : types[i].extensions) {
            if (!extensionOwner.contains(ext)) {
                extensionOwner.insert(ext, i);
            }
        }
        for (const QString &ext : types[i].shared) {
            sharedClaimants[ext] << i;
        }
    }

    // ---- 2a. almost all of the folder's own files have one type: skip the subfolders ----
    // Typical case: notes in Markdown (plus the odd PDF) next to a folder full of the images
    // they reference.
    if (contenders.isEmpty() && !root.ownFiles.isEmpty()) {
        QList<int> ownCounts(types.size(), 0);
        for (const QString &name : std::as_const(root.ownFiles)) {
            const int dot = name.lastIndexOf(QLatin1Char('.'));
            if (dot <= 0) {
                continue;
            }
            if (const auto it = extensionOwner.constFind(name.mid(dot + 1).toLower()); it != extensionOwner.constEnd()) {
                ++ownCounts[*it];
            }
        }
        const int owner = std::max_element(ownCounts.cbegin(), ownCounts.cend()) - ownCounts.cbegin();
        const int total = root.ownFiles.size();
        if (ownCounts[owner] >= m_settings.sameTypeMinFiles && ownCounts[owner] >= m_settings.sameTypeShare * total) {
            return {types[owner].id,
                    QStringLiteral("%1 of %2 files directly in the folder are %3").arg(ownCounts[owner]).arg(total).arg(types[owner].name)};
        }
    }

    // ---- 2b. the same test with every subfolder counted as one more item of its own type ----
    // A folder of Markdown notes and of subfolders that are themselves Markdown is Markdown.
    if (contenders.isEmpty() && !root.ownDirs.isEmpty() && level < m_settings.maxDepth) {
        QList<int> itemCounts(types.size(), 0);
        for (const QString &name : std::as_const(root.ownFiles)) {
            const int dot = name.lastIndexOf(QLatin1Char('.'));
            if (dot <= 0) {
                continue;
            }
            if (const auto it = extensionOwner.constFind(name.mid(dot + 1).toLower()); it != extensionOwner.constEnd()) {
                ++itemCounts[*it];
            }
        }
        const int total = root.ownFiles.size() + root.ownDirs.size();
        // Types still able to reach the share; once none can, stop classifying subfolders.
        int undecided = root.ownDirs.size();
        for (const QString &name : std::as_const(root.ownDirs)) {
            const int leader = *std::max_element(itemCounts.cbegin(), itemCounts.cend());
            if (budget <= 0 || leader + undecided < m_settings.sameTypeShare * total) {
                break;
            }
            --undecided;
            const QString subType = classifyAt(directory + QLatin1Char('/') + name, level + 1, budget).type;
            for (int i = 0; i < types.size() && !subType.isEmpty(); ++i) {
                if (types[i].id == subType) {
                    ++itemCounts[i];
                    break;
                }
            }
        }
        const int owner = std::max_element(itemCounts.cbegin(), itemCounts.cend()) - itemCounts.cbegin();
        if (itemCounts[owner] >= m_settings.sameTypeMinFiles && itemCounts[owner] >= m_settings.sameTypeShare * total) {
            return {types[owner].id,
                    QStringLiteral("%1 of %2 files and subfolders directly in the folder are %3").arg(itemCounts[owner]).arg(total).arg(types[owner].name)};
        }
    }

    // ---- 2c. dominant type of a bounded scan ----
    QList<int> counts(types.size(), 0);
    QHash<QString, int> sharedCounts;
    int totalFiles = 0;
    int entriesSeen = 0;

    // Breadth first, so that when the entry budget runs out the files closest to the folder
    // itself are the ones that were counted.
    std::deque<std::pair<fs::path, int>> queue;
    queue.emplace_back(rootPath, 0);
    while (!queue.empty() && entriesSeen < m_settings.maxEntries) {
        const auto [dir, depth] = queue.front();
        queue.pop_front();
        std::error_code iterEc;
        for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, iterEc), end; !iterEc && it != end; it.increment(iterEc)) {
            if (++entriesSeen > m_settings.maxEntries) {
                break;
            }
            const std::string rawName = it->path().filename().string();
            if (rawName.empty() || rawName[0] == '.') {
                continue;
            }
            std::error_code typeEc;
            if (it->is_symlink(typeEc)) {
                continue;
            }
            const QString name = QString::fromStdString(rawName);
            if (it->is_directory(typeEc)) {
                if (depth < m_settings.maxDepth && !ignored.contains(name)) {
                    queue.emplace_back(it->path(), depth + 1);
                }
                continue;
            }
            if (!it->is_regular_file(typeEc)) {
                continue;
            }
            ++totalFiles;
            const int dot = name.lastIndexOf(QLatin1Char('.'));
            if (dot <= 0) {
                continue;
            }
            const QString ext = name.mid(dot + 1).toLower();
            if (const auto owner = extensionOwner.constFind(ext); owner != extensionOwner.constEnd()) {
                ++counts[*owner];
            } else if (sharedClaimants.contains(ext)) {
                ++sharedCounts[ext];
            }
        }
    }

    budget -= entriesSeen;

    // Shared extensions (.h) go to whichever claimant is ahead on its own files.
    for (auto it = sharedCounts.constBegin(); it != sharedCounts.constEnd(); ++it) {
        const QList<int> claimants = sharedClaimants.value(it.key());
        int best = claimants.first();
        for (int claimant : claimants) {
            if (counts[claimant] > counts[best]) {
                best = claimant;
            }
        }
        counts[best] += it.value();
    }

    if (!contenders.isEmpty()) {
        int winner = contenders.first(); // table order breaks ties
        for (int contender : std::as_const(contenders)) {
            if (counts[contender] > counts[winner]) {
                winner = contender;
            }
        }
        return {types[winner].id,
                QStringLiteral("contains %1, %2 of %3 files are %4").arg(sharedMarker).arg(counts[winner]).arg(totalFiles).arg(types[winner].name)};
    }

    int best = -1;
    int runnerUp = 0;
    for (int i = 0; i < counts.size(); ++i) {
        if (best < 0 || counts[i] > counts[best]) {
            if (best >= 0) {
                runnerUp = qMax(runnerUp, counts[best]);
            }
            best = i;
        } else {
            runnerUp = qMax(runnerUp, counts[i]);
        }
    }
    if (best < 0 || counts[best] == 0 || totalFiles == 0) {
        return {QString(), QStringLiteral("no recognised files (%1 files seen)").arg(totalFiles)};
    }
    const double share = double(counts[best]) / totalFiles;
    const QString stats = QStringLiteral("%1 of %2 files are %3").arg(counts[best]).arg(totalFiles).arg(types[best].name);
    if (counts[best] == runnerUp) {
        return {QString(), stats + QStringLiteral(", tied with another type")};
    }
    if (share < m_settings.minShare) {
        return {QString(), stats + QStringLiteral(", below the %1% threshold").arg(qRound(m_settings.minShare * 100))};
    }
    return {types[best].id, stats};
}

}
