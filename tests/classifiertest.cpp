#include "classifier.h"
#include "store.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace FolderBadges;

class ClassifierTest : public QObject
{
    Q_OBJECT

private:
    static TypeTable builtinTable()
    {
        return TypeTable::builtin();
    }

    // Creates root/<name> with the given relative files (a trailing '/' makes a directory).
    static QString make(const QTemporaryDir &root, const QString &name, const QStringList &entries)
    {
        makeEntries(root.filePath(name), entries);
        return root.filePath(name);
    }

    static void makeEntries(const QString &base, const QStringList &entries)
    {
        QDir().mkpath(base);
        for (const QString &entry : entries) {
            const QString full = base + QLatin1Char('/') + entry;
            if (entry.endsWith(QLatin1Char('/'))) {
                QDir().mkpath(full);
            } else {
                QDir().mkpath(QFileInfo(full).path());
                QFile file(full);
                QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(full));
            }
        }
    }

private Q_SLOTS:
    void builtinTableLoads()
    {
        const TypeTable table = builtinTable();
        QVERIFY(table.types().size() > 20);
        QCOMPARE(table.iconsFor(QStringLiteral("rust")).first(), QStringLiteral("text-rust"));
        QVERIFY(table.iconsFor(QStringLiteral("nope")).isEmpty());
    }

    void classify_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QStringList>("entries");
        QTest::addColumn<QString>("expected");

        QTest::newRow("cargo marker") << "server" << QStringList{"Cargo.toml", "README.md", "docs/a.md", "docs/b.md"} << "rust";
        QTest::newRow("ts beats js") << "web" << QStringList{"package.json", "tsconfig.json", "index.js"} << "typescript";
        QTest::newRow("plain js") << "web2" << QStringList{"package.json", "index.js"} << "javascript";
        QTest::newRow("nix flake") << "sys" << QStringList{"flake.nix", "a.sh", "b.sh", "c.sh"} << "nix";
        QTest::newRow("neovim config") << "cfg" << QStringList{"init.lua", "lua/x.lua"} << "neovim";
        QTest::newRow("neovim by name") << "nvim" << QStringList{"x.txt"} << "neovim";
        QTest::newRow("neovim plugin name") << "telescope.nvim" << QStringList{"lua/t.lua"} << "neovim";
        QTest::newRow("neovim dir markers") << "plug" << QStringList{"lua/", "plugin/", "x.txt"} << "neovim";
        QTest::newRow("termux by name") << "termux-packages" << QStringList{"build.sh"} << "termux";
        QTest::newRow("kernel-like") << "kernel" << QStringList{"Kconfig", "Makefile", "a.py"} << "c";
        QTest::newRow("markdown notes") << "physics" << QStringList{"a.md", "b.md", "img/fig.png"} << "markdown";
        QTest::newRow("c with headers") << "cproj" << QStringList{"a.c", "b.c", "a.h", "b.h", "c.h", "x.cpp"} << "c";
        QTest::newRow("c++ with headers") << "cppproj" << QStringList{"a.cpp", "b.cpp", "a.h", "b.h", "c.h", "x.c"} << "cpp";
        QTest::newRow("lua, not neovim") << "game" << QStringList{"main.lua", "conf.lua"} << "lua";
        QTest::newRow("photos") << "holiday" << QStringList{"1.JPG", "2.jpg", "3.png", "notes.md"} << "images";
        QTest::newRow("empty") << "empty" << QStringList{} << "";
        QTest::newRow("unknown files") << "misc" << QStringList{"a.xyz", "b.qqq", "c"} << "";
        QTest::newRow("below threshold") << "mixed" << QStringList{"a.md", "b.xyz", "c.xyz", "d.xyz"} << "";
        QTest::newRow("tie") << "tie" << QStringList{"a.md", "b.py"} << "";
        QTest::newRow("ignored dirs") << "ign" << QStringList{"a.md", "node_modules/a.js", "node_modules/b.js", ".git/x.py", ".git/y.py"}
                                      << "markdown";
        QTest::newRow("marker needs dir") << "notdir" << QStringList{"lua", "plugin", "a.md", "b.md"} << "markdown";
    }

    void classify()
    {
        QFETCH(QString, name);
        QFETCH(QStringList, entries);
        QFETCH(QString, expected);

        QTemporaryDir root;
        QVERIFY(root.isValid());
        const Classifier classifier(builtinTable(), Settings());
        const Classification result = classifier.classify(make(root, name, entries));
        QVERIFY2(result.type == expected, qPrintable(QStringLiteral("got '%1' (%2)").arg(result.type, result.reason)));
    }

    void depthAndEntryLimits()
    {
        QTemporaryDir root;
        Settings settings;
        settings.maxDepth = 1;
        const QString dir = make(root, QStringLiteral("deep"), {"a.md", "x/b.py", "x/y/c.py", "x/y/d.py", "x/y/e.py"});
        // Only a.md and x/b.py are within depth 1 -> tie -> no badge.
        QCOMPARE(Classifier(builtinTable(), settings).classify(dir).type, QString());
        settings.maxDepth = 4;
        QCOMPARE(Classifier(builtinTable(), settings).classify(dir).type, QStringLiteral("python"));
    }

    void userTypesOverride()
    {
        TypeTable table = builtinTable();
        const int before = table.types().size();
        table.merge(TypeTable::fromJson(R"({"types": [
            {"id": "rust", "icons": ["my-rust"], "markers": ["Cargo.toml"]},
            {"id": "zig", "icons": ["text-x-zig"], "markers": ["build.zig"], "extensions": ["zig"]}
        ]})"));
        QCOMPARE(table.types().size(), before + 1);
        QCOMPARE(table.iconsFor(QStringLiteral("rust")), QStringList{"my-rust"});
        QCOMPARE(table.types().first().id, QStringLiteral("zig"));
    }

    void storeRoundTrip()
    {
        // QTemporaryDir lives in /tmp, where tmpfs may refuse user.* xattrs; use the build dir.
        QTemporaryDir root(QDir::currentPath() + QStringLiteral("/storetest-XXXXXX"));
        QVERIFY(root.isValid());
        const QString dir = root.path();
        QVERIFY(!Store::read(dir));
        if (!Store::write(dir, StoredBadge{QStringLiteral("rust"), 1234, true})) {
            QSKIP("filesystem does not support user extended attributes");
        }
        auto stored = Store::read(dir);
        QVERIFY(stored);
        QCOMPARE(stored->type, QStringLiteral("rust"));
        QCOMPARE(stored->stamp, 1234);
        QVERIFY(stored->pinned);

        QVERIFY(Store::write(dir, StoredBadge{QString(), 99, false}));
        stored = Store::read(dir);
        QVERIFY(stored && stored->type.isEmpty() && !stored->pinned);

        QVERIFY(Store::clear(dir));
        QVERIFY(!Store::read(dir));
        QVERIFY(Store::clear(dir)); // clearing twice is fine
    }
};

QTEST_GUILESS_MAIN(ClassifierTest)
#include "classifiertest.moc"
