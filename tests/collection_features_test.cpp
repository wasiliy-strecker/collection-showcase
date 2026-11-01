#include "application/collection_codec.h"
#include "application/collection_transfer.h"
#include "i18n/languages.h"
#include "persistence/json_collection_file.h"
#include "persistence/sqlite_item_repository.h"
#include "ui/main_window.h"
#include <QBuffer>
#include <QHeaderView>
#include <QImage>
#include <QJsonArray>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>
using namespace vitrine;
namespace {
ItemDraft richDraft() {
  ItemDraft draft;
  draft.name = QStringLiteral("Étoile 東京");
  draft.maker = QStringLiteral("Material 12");
  draft.location = QStringLiteral("Shelf 2");
  draft.note = QStringLiteral("First line\nSecond line");
  QImage image(40, 20, QImage::Format_RGB32);
  image.fill(Qt::red);
  QBuffer buffer(&draft.photo);
  buffer.open(QIODevice::WriteOnly);
  if (!image.save(&buffer, "PNG"))
    throw std::runtime_error("Cannot create fixture");
  return draft;
}
} // namespace
class CollectionFeaturesTest final : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() { QLocale::setDefault(QLocale(QStringLiteral("en_US"))); }
  void completeSnapshotAndRepeatedImport() {
    QTemporaryDir data, destination, output;
    const auto draft = richDraft();
    {
      SqliteItemRepository repository(data.path());
      repository.initialize();
      const auto first = repository.create(draft);
      QVERIFY(repository.findAll().first().fields.photo.isEmpty());
      QCOMPARE(repository.transferSnapshot(), TransferDraftList{draft});
      JsonCollectionFile file(data.path());
      CollectionTransfer transfer(repository, file);
      const auto path = output.filePath(QStringLiteral("collection.json"));
      QCOMPARE(transfer.exportEntries(file.inspect(path), false), 1);
      QCOMPARE(file.read(path), TransferDraftList{draft});
      SqliteItemRepository imported(destination.path());
      imported.initialize();
      const auto result = imported.importEntries(file.read(path));
      QCOMPARE(result.added, 1);
      QCOMPARE(result.skipped, 0);
      QCOMPARE(imported.transferSnapshot(), TransferDraftList{draft});
      QCOMPARE(imported.importEntries(file.read(path)).added, 0);
      QCOMPARE(repository.findAll().first().id, first.id);
    }
    SqliteItemRepository reopened(destination.path());
    reopened.initialize();
    QCOMPARE(reopened.transferSnapshot(), TransferDraftList{draft});
  }
  void contentDifferencesAreNotDuplicates() {
    QTemporaryDir data;
    SqliteItemRepository repository(data.path());
    repository.initialize();
    const auto draft = richDraft();
    repository.create(draft);
    auto changed = draft;
    changed.photo.clear();
    QCOMPARE(repository.importEntries({changed}).added, 1);
  }
  void invalidPhotoAndMetadataAreRejected() {
    auto object = encodeEntry(richDraft());
    object.insert(QStringLiteral("photoBase64"), QStringLiteral("YWJj"));
    QVERIFY_EXCEPTION_THROWN(decodeEntry(object), std::exception);
    object = encodeEntry(richDraft());
    object.insert(QStringLiteral("photoBase64"), QStringLiteral("!!"));
    QVERIFY_EXCEPTION_THROWN(decodeEntry(object), std::exception);
  }
  void sortingKeepsSelectionAndPhotoDetails_data() {
    QTest::addColumn<QString>("tag");
    for (const auto& tag : supportedLanguages())
      QTest::newRow(qPrintable(tag)) << tag;
  }
  void sortingKeepsSelectionAndPhotoDetails() {
    QFETCH(QString, tag);
    Translations translations;
    translations.install(tag);
    QTemporaryDir data;
    qint64 selectedId = 0;
    {
      SqliteItemRepository repository(data.path());
      repository.initialize();
      auto one = richDraft();
      one.name = QStringLiteral("Entry 10");
      selectedId = repository.create(one).id;
      auto two = one;
      two.name = QStringLiteral("Entry 2");
      two.photo.clear();
      repository.create(two);
    }
    StorageController controller(
        [&data] { return std::make_unique<SqliteItemRepository>(data.path()); });
    QSignalSpy loaded(&controller, &StorageController::loaded);
    MainWindow window(controller, data.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }));
    auto* table = window.findChild<QTableWidget*>(QStringLiteral("itemsTable"));
    QVERIFY(table);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("Entry 2"));
    table->selectRow(1);
    table->horizontalHeader()->setSortIndicator(3, Qt::DescendingOrder);
    QCOMPARE(table->currentRow(), 0);
    QCOMPARE(table->item(0, 0)->data(Qt::UserRole).toLongLong(), selectedId);
    for (int column = 0; column < table->columnCount(); ++column) {
      for (auto order : {Qt::AscendingOrder, Qt::DescendingOrder}) {
        table->horizontalHeader()->setSortIndicator(column, order);
        QCOMPARE(table->item(table->currentRow(), 0)->data(Qt::UserRole).toLongLong(), selectedId);
      }
    }
    const auto captures = qEnvironmentVariable("CPP_COLLECTION_SCREENSHOTS");
    if (!captures.isEmpty()) {
      QDir().mkpath(captures);
      QTest::qWait(250);
      QVERIFY(window.grab().save(QDir(captures).filePath(tag + QStringLiteral("-collection.png"))));
    }
    window.resize(560, 520);
    auto* scroll = qobject_cast<QScrollArea*>(window.centralWidget());
    QVERIFY(scroll);
    for (const auto* name : {"exportCollection", "importCollection", "aboutApp"}) {
      auto* button = window.findChild<QPushButton*>(QString::fromLatin1(name));
      QVERIFY(button);
      scroll->ensureWidgetVisible(button);
      QVERIFY(button->isEnabled());
    }
    if (!captures.isEmpty()) {
      QTest::qWait(100);
      QVERIFY(window.grab().save(QDir(captures).filePath(tag + QStringLiteral("-small.png"))));
    }
  }
};
QTEST_MAIN(CollectionFeaturesTest)
#include "collection_features_test.moc"
