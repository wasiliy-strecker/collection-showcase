#include "application/item_service.h"
#include "persistence/sqlite_item_repository.h"

#include "application/photo_importer.h"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

using namespace vitrine;

namespace {
QString databasePath(const QTemporaryDir& directory) {
  return QDir(directory.path()).filePath(QStringLiteral("sammlervitrine.sqlite3"));
}

QByteArray photoBytes(const QColor& color) {
  QImage image(30, 20, QImage::Format_RGB32);
  image.fill(color);
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return bytes;
}

class ExternalDatabase final {
public:
  explicit ExternalDatabase(const QString& path) {
    name_ = QUuid::createUuid().toString();
    database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name_);
    database.setDatabaseName(path);
    database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=50"));
    if (!database.open()) {
      throw StorageError(database.lastError().text());
    }
  }
  ~ExternalDatabase() {
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(name_);
  }
  void execute(const QString& sql) {
    QSqlQuery query(database);
    if (!query.exec(sql)) {
      throw StorageError(query.lastError().text());
    }
  }
  QSqlDatabase database;

private:
  QString name_;
};

QByteArray readFile(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}
} // namespace

class RepositoryTest final : public QObject {
  Q_OBJECT
private slots:
  void photosPersistIndependentlyWithoutSources() {
    QTemporaryDir directory;
    const auto red = photoBytes(Qt::red);
    const auto blue = photoBytes(Qt::blue);
    qint64 firstId = 0;
    qint64 secondId = 0;
    {
      QFile source(directory.filePath("source.jpg"));
      QVERIFY(source.open(QIODevice::WriteOnly));
      source.write(red);
      source.close();
      SqliteItemRepository repository(directory.path());
      repository.initialize();
      auto first = repository.create({QStringLiteral("Figur"),
                                      {},
                                      {},
                                      QStringLiteral("Notiz"),
                                      importPhoto(source.fileName()).bytes});
      auto second = repository.create({QStringLiteral("Figur"), {}, {}, {}, blue});
      firstId = first.id;
      secondId = second.id;
      QVERIFY(firstId != secondId);
      QVERIFY(source.remove());
      for (const auto& summary : repository.findAll()) {
        QVERIFY(summary.hasPhoto);
        QVERIFY(summary.fields.photo.isEmpty());
      }
    }
    {
      SqliteItemRepository repository(directory.path());
      repository.initialize();
      QCOMPARE(repository.find(firstId).fields.photo, red);
      QCOMPARE(repository.find(secondId).fields.photo, blue);
      auto draft = repository.find(firstId).fields;
      draft.photo = blue;
      repository.update(firstId, draft);
      QCOMPARE(repository.find(firstId).fields.photo, blue);
      draft.photo.clear();
      repository.update(firstId, draft);
      QVERIFY(!repository.find(firstId).hasPhoto);
      QCOMPARE(repository.find(secondId).fields.photo, blue);
      repository.remove(secondId);
      QVERIFY_EXCEPTION_THROWN(repository.find(secondId), StorageError);
      QCOMPARE(repository.findAll().size(), 1);
    }
  }
  void failedPhotoCommitIsAtomicAndSameDraftRetries() {
    QTemporaryDir directory;
    SqliteItemRepository repository(directory.path());
    repository.initialize();
    const auto original =
        repository.create({QStringLiteral("Original"), QStringLiteral("Marke"),
                           QStringLiteral("Ort"), QStringLiteral("Notiz"), photoBytes(Qt::red)});
    auto draft = original.fields;
    draft.name = QStringLiteral("Neu");
    draft.note = QStringLiteral("Neue\nNotiz");
    draft.photo = photoBytes(Qt::blue);
    ExternalDatabase reader(databasePath(directory));
    reader.execute(QStringLiteral("BEGIN"));
    reader.execute(QStringLiteral("SELECT * FROM items"));
    QVERIFY_EXCEPTION_THROWN(repository.update(original.id, draft), StorageError);
    reader.execute(QStringLiteral("ROLLBACK"));
    QCOMPARE(repository.find(original.id), original);
    repository.update(original.id, draft);
    QCOMPARE(repository.find(original.id).fields, draft);
    reader.execute(QStringLiteral("BEGIN EXCLUSIVE"));
    auto removed = draft;
    removed.photo.clear();
    QVERIFY_EXCEPTION_THROWN(repository.update(original.id, removed), StorageError);
    QVERIFY_EXCEPTION_THROWN(repository.remove(original.id), StorageError);
    reader.execute(QStringLiteral("ROLLBACK"));
    QCOMPARE(repository.find(original.id).fields, draft);
  }
  void invalidStoredBlobDoesNotBecomeMissingPhoto() {
    QTemporaryDir directory;
    SqliteItemRepository repository(directory.path());
    repository.initialize();
    const auto item = repository.create({QStringLiteral("Figur"), {}, {}, {}});
    ExternalDatabase external(databasePath(directory));
    external.execute(QStringLiteral("UPDATE items SET photo=zeroblob(10485761)"));
    QVERIFY(repository.findAll().first().hasPhoto);
    QVERIFY_EXCEPTION_THROWN(repository.find(item.id), StorageError);
    external.execute(QStringLiteral("UPDATE items SET photo=x''"));
    QVERIFY_EXCEPTION_THROWN(repository.find(item.id), StorageError);
    external.execute(QStringLiteral("UPDATE items SET photo='wrong type'"));
    QVERIFY_EXCEPTION_THROWN(repository.find(item.id), StorageError);
  }
  void foreignApplicationIdRemainsUntouched() {
    QTemporaryDir directory;
    {
      SqliteItemRepository repository(directory.path());
      repository.initialize();
    }
    {
      ExternalDatabase external(databasePath(directory));
      external.execute(QStringLiteral("PRAGMA application_id=42"));
    }
    const auto before = readFile(databasePath(directory));
    SqliteItemRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
    QCOMPARE(readFile(databasePath(directory)), before);
  }
  void crudAndIndependentDuplicates() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteItemRepository repository(directory.path());
    ItemService service(repository);
    QVERIFY(service.load().isEmpty());
    const auto first = service.save(0, {QStringLiteral("Azul"), {}, {}, {}});
    const auto second = service.save(0, first.fields);
    QVERIFY(first.id > 0);
    QVERIFY(first.id != second.id);
    auto changed = first.fields;
    changed.location = QStringLiteral("Regal 4");
    const auto updated = service.save(first.id, changed);
    QCOMPARE(updated.id, first.id);
    const auto items = service.load();
    QCOMPARE(items.size(), 2);
    QVERIFY(items.first() == updated);
    QVERIFY(items.last() == second);
    service.remove(first.id);
    QVERIFY(service.load() == ItemList{second});
    service.remove(second.id);
    QVERIFY(service.load().isEmpty());
    QVERIFY(service.save(0, second.fields).id > second.id);
  }
  void reopensWithAllFieldsAndSqlText() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Collectible saved;
    const ItemDraft fields{
        QStringLiteral("  Ömer's Sammlerstück'); DROP TABLE items; --  "),
        QStringLiteral("Müller & \"Söhne\""), QStringLiteral("Küche\u00a0"),
        QStringLiteral("  Erweiterung liegt hier.\n\n\tSELECT * FROM items;\n ")};
    {
      SqliteItemRepository repository(directory.path());
      ItemService service(repository);
      service.load();
      saved = service.save(0, fields);
    }
    {
      SqliteItemRepository repository(directory.path());
      ItemService service(repository);
      const auto items = service.load();
      QCOMPARE(items.size(), 1);
      QVERIFY(items.first() == saved);
      QCOMPARE(items.first().fields.note, fields.note);
      service.save(0, {QStringLiteral("Noch ein Sammlerstück"), {}, {}, {}});
      QCOMPARE(service.load().size(), 2);
    }
  }
  void invalidAndMissingEntriesDoNotChangeData() {
    QTemporaryDir directory;
    SqliteItemRepository repository(directory.path());
    ItemService service(repository);
    service.load();
    const auto saved = service.save(0, {QStringLiteral("Unverändert"), {}, {}, {}});
    QVERIFY_EXCEPTION_THROWN(service.save(saved.id, {QStringLiteral("\u00a0"), {}, {}, {}}),
                             ValidationError);
    QVERIFY_EXCEPTION_THROWN(service.save(999, saved.fields), StorageError);
    QVERIFY_EXCEPTION_THROWN(service.remove(999), StorageError);
    QVERIFY(service.load() == ItemList{saved});
  }
  void lockedWriteAndRetry() {
    QTemporaryDir directory;
    SqliteItemRepository repository(directory.path());
    ItemService service(repository);
    service.load();
    const auto saved = service.save(0, {QStringLiteral("Original"), {}, {}, {}});
    ExternalDatabase external(databasePath(directory));
    external.execute(QStringLiteral("BEGIN EXCLUSIVE"));
    QVERIFY_EXCEPTION_THROWN(service.save(saved.id, {QStringLiteral("Entwurf"), {}, {}, {}}),
                             StorageError);
    QVERIFY_EXCEPTION_THROWN(service.remove(saved.id), StorageError);
    external.execute(QStringLiteral("ROLLBACK"));
    QVERIFY(service.load() == ItemList{saved});
    QCOMPARE(service.save(saved.id, {QStringLiteral("Entwurf"), {}, {}, {}}).fields.name,
             QStringLiteral("Entwurf"));
  }
  void failedCommitRollsBackAllFields() {
    QTemporaryDir directory;
    SqliteItemRepository repository(directory.path());
    ItemService service(repository);
    service.load();
    const auto saved = service.save(0, {QStringLiteral("Original"), QStringLiteral("Marke"),
                                        QStringLiteral("Regal"), QStringLiteral("Notiz")});
    ExternalDatabase reader(databasePath(directory));
    reader.execute(QStringLiteral("BEGIN"));
    reader.execute(QStringLiteral("SELECT * FROM items"));
    QVERIFY_EXCEPTION_THROWN(service.save(saved.id, {QStringLiteral("Neu"), QStringLiteral("A"),
                                                     QStringLiteral("B"), QStringLiteral("C")}),
                             StorageError);
    reader.execute(QStringLiteral("ROLLBACK"));
    QVERIFY(service.load() == ItemList{saved});
    QVERIFY_EXCEPTION_THROWN(service.save(999, saved.fields), StorageError);
    QCOMPARE(service.save(saved.id, {QStringLiteral("Erneut"), {}, {}, {}}).id, saved.id);
  }
  void triggerWriteFailurePreservesData() {
    QTemporaryDir directory;
    SqliteItemRepository repository(directory.path());
    ItemService service(repository);
    service.load();
    const auto saved = service.save(0, {QStringLiteral("Original"), {}, {}, {}});
    ExternalDatabase external(databasePath(directory));
    external.execute(QStringLiteral("CREATE TRIGGER reject_update BEFORE UPDATE ON items BEGIN "
                                    "SELECT RAISE(ABORT, 'synthetischer Schreibfehler'); END"));
    QVERIFY_EXCEPTION_THROWN(service.save(saved.id, {QStringLiteral("Neu"), {}, {}, {}}),
                             StorageError);
    QVERIFY(service.load() == ItemList{saved});
    external.execute(QStringLiteral("DROP TRIGGER reject_update"));
    QCOMPARE(service.save(saved.id, {QStringLiteral("Neu"), {}, {}, {}}).id, saved.id);
  }
  void newerSchemaUnchanged() {
    QTemporaryDir directory;
    {
      ExternalDatabase database(databasePath(directory));
      database.execute(QStringLiteral("PRAGMA user_version=99"));
      database.execute(QStringLiteral("CREATE TABLE future(value TEXT)"));
      database.execute(QStringLiteral("INSERT INTO future VALUES('synthetisch')"));
    }
    const auto before = readFile(databasePath(directory));
    SqliteItemRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
    QCOMPARE(readFile(databasePath(directory)), before);
  }
  void corruptDatabaseUnchanged() {
    QTemporaryDir directory;
    QFile file(databasePath(directory));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Synthetisch beschaedigt. Keine SQLite-Datei.");
    file.close();
    const auto before = readFile(databasePath(directory));
    SqliteItemRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
    QCOMPARE(readFile(databasePath(directory)), before);
  }
  void existingEmptyOrForeignDatabase_data() {
    QTest::addColumn<bool>("foreignSchema");
    QTest::newRow("empty-existing-file") << false;
    QTest::newRow("foreign-schema") << true;
  }
  void existingEmptyOrForeignDatabase() {
    QFETCH(bool, foreignSchema);
    QTemporaryDir directory;
    if (foreignSchema) {
      ExternalDatabase database(databasePath(directory));
      database.execute(QStringLiteral("CREATE TABLE unrelated(value TEXT)"));
    } else {
      QFile file(databasePath(directory));
      QVERIFY(file.open(QIODevice::WriteOnly));
    }
    const auto before = readFile(databasePath(directory));
    SqliteItemRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
    QCOMPARE(readFile(databasePath(directory)), before);
  }
  void wrongSchemaVersionOne() {
    QTemporaryDir directory;
    {
      ExternalDatabase database(databasePath(directory));
      database.execute(QStringLiteral("PRAGMA user_version=1"));
      database.execute(QStringLiteral("PRAGMA application_id=1398166612"));
      database.execute(QStringLiteral("CREATE TABLE items(wrong TEXT)"));
    }
    SqliteItemRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
  }
  void wrongTypesInKnownSchema() {
    QTemporaryDir directory;
    {
      ExternalDatabase database(databasePath(directory));
      database.execute(QStringLiteral("PRAGMA user_version=1"));
      database.execute(QStringLiteral("PRAGMA application_id=1398166612"));
      database.execute(QStringLiteral(
          "CREATE TABLE items(id TEXT,name TEXT,maker TEXT,location TEXT,note TEXT)"));
    }
    const auto before = readFile(databasePath(directory));
    SqliteItemRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
    QCOMPARE(readFile(databasePath(directory)), before);
  }
  void lockedStartupCanRetry() {
    QTemporaryDir directory;
    {
      SqliteItemRepository repository(directory.path());
      repository.initialize();
      repository.create({QStringLiteral("Vorhanden"), {}, {}, {}});
    }
    ExternalDatabase blocker(databasePath(directory));
    blocker.execute(QStringLiteral("BEGIN EXCLUSIVE"));
    SqliteItemRepository repository(directory.path());
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
    blocker.execute(QStringLiteral("ROLLBACK"));
    repository.initialize();
    QCOMPARE(repository.findAll().first().fields.name, QStringLiteral("Vorhanden"));
  }
  void secondInstanceRefusedThenRetries() {
    QTemporaryDir directory;
    auto first = std::make_unique<SqliteItemRepository>(directory.path());
    first->initialize();
    SqliteItemRepository second(directory.path());
    QVERIFY_EXCEPTION_THROWN(second.initialize(), StorageError);
    first.reset();
    second.initialize();
    QVERIFY(second.findAll().isEmpty());
  }
  void invalidDirectory() {
    QTemporaryDir directory;
    const auto path = QDir(directory.path()).filePath(QStringLiteral("file"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    SqliteItemRepository repository(path);
    QVERIFY_EXCEPTION_THROWN(repository.initialize(), StorageError);
  }
  void readOnlyFile() {
#ifdef Q_OS_WIN
    QSKIP("Windows-Schreibschutz erfordert einen nativen ACL-Test.");
#else
    QTemporaryDir directory;
    Collectible saved;
    {
      SqliteItemRepository repository(directory.path());
      ItemService service(repository);
      service.load();
      saved = service.save(0, {QStringLiteral("Original"), {}, {}, {}});
    }
    QVERIFY(QFile::setPermissions(databasePath(directory), QFileDevice::ReadOwner));
    {
      SqliteItemRepository repository(directory.path());
      ItemService service(repository);
      QVERIFY(service.load() == ItemList{saved});
      QVERIFY_EXCEPTION_THROWN(service.save(saved.id, {QStringLiteral("Neu"), {}, {}, {}}),
                               StorageError);
      QVERIFY(service.load() == ItemList{saved});
    }
    QVERIFY(QFile::setPermissions(databasePath(directory),
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner));
#endif
  }
};

QTEST_GUILESS_MAIN(RepositoryTest)
#include "repository_test.moc"
