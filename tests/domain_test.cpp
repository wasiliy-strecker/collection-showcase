#include "application/item_repository.h"
#include "domain/item.h"
#include "persistence/data_directory.h"

#include <QLocale>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using namespace vitrine;

class DomainTest final : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("sammlervitrine-tests"));
  }
  void requiredName_data() {
    QTest::addColumn<QString>("name");
    QTest::newRow("empty") << QString();
    QTest::newRow("ascii") << QStringLiteral(" \t\r\n ");
    QTest::newRow("nbsp") << QString(QChar(0x00a0));
    QTest::newRow("figure-space") << QString(QChar(0x2007));
    QTest::newRow("narrow-nbsp") << QString(QChar(0x202f));
    QTest::newRow("ideographic") << QString(QChar(0x3000));
    QTest::newRow("mixed") << QStringLiteral("\u2002 \u2003\u0085\u2028\u2029");
  }
  void requiredName() {
    QFETCH(QString, name);
    QVERIFY_EXCEPTION_THROWN(validateDraft({name, {}, {}, {}}), ValidationError);
  }
  void trimsSingleLinesPreservesNote() {
    const QString note = QStringLiteral("  Erste Zeile\n\n\tGeschenk von Ömer.\n ");
    const auto draft =
        validateDraft({QStringLiteral("\u00a0 Azul \u3000"), QStringLiteral("  Marke\u202f"),
                       QStringLiteral("\t Regal 2 "), note});
    QCOMPARE(draft.name, QStringLiteral("Azul"));
    QCOMPARE(draft.maker, QStringLiteral("Marke"));
    QCOMPARE(draft.location, QStringLiteral("Regal 2"));
    QCOMPARE(draft.note, note);
    const auto minimal = validateDraft({QStringLiteral("Sammlerstück"), {}, {}, {}});
    QVERIFY(minimal.maker.isEmpty());
    QVERIFY(minimal.location.isEmpty());
    QVERIFY(minimal.note.isEmpty());
  }
  void searchEveryField_data() {
    QTest::addColumn<QString>("query");
    QTest::newRow("name") << QStringLiteral("ÄRGER");
    QTest::newRow("maker") << QStringLiteral("mÜllEr");
    QTest::newRow("location") << QStringLiteral("KÜCHE");
    QTest::newRow("note") << QStringLiteral("ÖMER");
    QTest::newRow("decomposed-query") << QStringLiteral("O\u0308mer");
  }
  void searchEveryField() {
    QFETCH(QString, query);
    const ItemList items{{42,
                          {QStringLiteral("Ärger"), QStringLiteral("Müller"),
                           QStringLiteral("Küche"), QStringLiteral("Geschenk von Ömer.")}}};
    QCOMPARE(searchItems(items, query).size(), 1);
    QCOMPARE(searchItems(items, query).first().id, 42);
  }
  void normalizationInStoredText() {
    const ItemList items{
        {7, {QStringLiteral("A\u0308rger"), {}, {}, QStringLiteral("O\u0308mer")}}};
    QCOMPARE(searchItems(items, QStringLiteral("ÄRGER")).size(), 1);
    QCOMPARE(searchItems(items, QStringLiteral("ömer")).size(), 1);
  }
  void emptyMissingAndStableOrder() {
    const auto previous = QLocale();
    const auto restore = qScopeGuard([previous] { QLocale::setDefault(previous); });
    QLocale::setDefault(QLocale(QStringLiteral("de-DE")));
    const ItemList items{{9, {QStringLiteral("Sammlerstück 10"), {}, {}, {}}},
                         {8, {QStringLiteral("Azul"), {}, {}, {}}},
                         {4, {QStringLiteral("azul"), {}, {}, {}}},
                         {5, {QStringLiteral("Sammlerstück 2"), {}, {}, {}}}};
    const auto result = searchItems(items, QStringLiteral("\u00a0 "));
    QCOMPARE(result.size(), 4);
    QCOMPARE(result.at(0).id, 4);
    QCOMPARE(result.at(1).id, 8);
    QCOMPARE(result.at(2).id, 5);
    QCOMPARE(result.at(3).id, 9);
    QVERIFY(searchItems(items, QStringLiteral("nicht vorhanden")).isEmpty());
    QVERIFY(searchItems({}, {}).isEmpty());
  }
  void isolatedDataDirectory() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QCOMPARE(resolveDataDirectory(directory.path(), true), directory.path());
    QVERIFY_EXCEPTION_THROWN(resolveDataDirectory({}, true), StorageError);
    QVERIFY_EXCEPTION_THROWN(resolveDataDirectory(QStringLiteral("relative"), true), StorageError);
    QVERIFY(!defaultDataDirectory().isEmpty());
    QVERIFY(defaultDataDirectory().contains(QStringLiteral("sammlervitrine-tests")));
  }
};

QTEST_GUILESS_MAIN(DomainTest)
#include "domain_test.moc"
