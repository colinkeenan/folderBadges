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
        QCOMPARE(table.iconsFor(QStringLiteral("rust")).first(), QStringLiteral("folderbadges-rust"));
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
        QTest::newRow("md with image folder") << "guide" << QStringList{"a.md", "b.md", "img/1.png", "img/2.png", "img/3.png", "img/4.png", "img/5.png"}
                                              << "markdown";
        QTest::newRow("md, hidden file ignored") << "guide2" << QStringList{"a.md", "b.md", ".directory", "img/1.png", "img/2.png", "img/3.png"}
                                                 << "markdown";
        QTest::newRow("one md is not enough") << "readme" << QStringList{"README.md", "src/a.py", "src/b.py", "src/c.py"} << "python";
        QTest::newRow("mostly md plus a pdf") << "book" << QStringList{"a.md", "b.md", "c.md", "book.pdf", "img/1.png", "img/2.png", "img/3.png",
                                                                       "img/4.png", "img/5.png", "img/6.png"}
                                              << "markdown";
        QTest::newRow("md plus other file") << "mixedroot" << QStringList{"a.md", "b.md", "x.dat", "img/1.png", "img/2.png", "img/3.png", "img/4.png"}
                                            << "images";
        QTest::newRow("cmake c++ with assets") << "cmakecpp"
                                               << QStringList{"CMakeLists.txt", "src/a.cpp", "src/a.h", "art/1.svg", "art/2.svg", "art/3.svg", "art/4.svg"}
                                               << "cpp";
        QTest::newRow("cmake c") << "cmakec" << QStringList{"CMakeLists.txt", "src/a.c", "src/b.c", "src/x.cpp", "src/a.h"} << "c";
        QTest::newRow("cmake, no sources yet") << "cmakenew" << QStringList{"CMakeLists.txt", "README.md"} << "cpp";
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

    void symlinkDoesNotBreakSameTypeRule()
    {
        // Notes with a "book.pdf" link to the source next to them are still Markdown.
        QTemporaryDir root;
        const QString dir = make(root, QStringLiteral("book"), {"a.md", "b.md", "images/1.png", "images/2.png", "images/3.png", "images/4.png"});
        make(root, QStringLiteral("elsewhere"), {"source.pdf"});
        QVERIFY(QFile::link(root.filePath(QStringLiteral("elsewhere/source.pdf")), dir + QStringLiteral("/book.pdf")));
        const Classification result = Classifier(builtinTable(), Settings()).classify(dir);
        QVERIFY2(result.type == QStringLiteral("markdown"), qPrintable(result.reason));
    }

    void subfoldersCountAsItems()
    {
        QTemporaryDir root;
        // Own files alone: 2 of 4 Markdown, not enough. Each "bookN" subfolder is Markdown
        // (despite its many images), "misc" is not: 6 of 9 items, still short of 75%.
        QStringList entries{"a.md", "b.md", "x.pdf", "y.txt", "misc/data.bin"};
        for (int i = 1; i <= 4; ++i) {
            for (const char *file : {"1.md", "2.md", "images/a.png", "images/b.png", "images/c.png", "images/d.png", "images/e.png"}) {
                entries << QStringLiteral("book%1/%2").arg(i).arg(QLatin1String(file));
            }
        }
        const QString dir = make(root, QStringLiteral("physics"), entries);
        const Classifier classifier(builtinTable(), Settings());
        QCOMPARE(classifier.classify(dir).type, QStringLiteral("images")); // decided by the scan

        // Two more Markdown subfolders: 8 of 11 items, still short; a third makes 9 of 12.
        makeEntries(dir, {"book5/1.md", "book5/2.md", "book6/1.md", "book6/2.md"});
        QCOMPARE(classifier.classify(dir).type, QStringLiteral("images"));
        makeEntries(dir, {"book7/1.md", "book7/2.md"});
        const Classification result = classifier.classify(dir);
        QVERIFY2(result.type == QStringLiteral("markdown"), qPrintable(result.reason));
        QVERIFY(result.reason.contains(QStringLiteral("9 of 12 files and subfolders")));
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
