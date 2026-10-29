#include "persistence/sqlite_item_repository.h"
#include "application/photo_importer.h"
#include <QCoreApplication>

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <utility>

namespace vitrine {
namespace {

[[noreturn]] void fail(const QSqlError& error) {
  // SQLite extended result codes keep the primary result in the low byte.
  const int code = error.nativeErrorCode().toInt() & 0xff;
  if (code == 5 || code == 6) {
    throw StorageError(QCoreApplication::translate(
        "App", "Die Datenbank ist gerade gesperrt. Beende andere Zugriffe "
               "und versuche es erneut. Deine Eingaben bleiben erhalten."));
  }
  if (code == 11 || code == 26) {
    throw StorageError(QCoreApplication::translate(
        "App", "Die Datenbank ist beschädigt oder keine gültige SQLite-Datei. "
               "Sie wird nicht ersetzt. Sichere die Datei vor einer Reparatur."));
  }
  if (code == 3 || code == 8 || code == 14) {
    throw StorageError(QCoreApplication::translate(
        "App", "Die Datenbank kann nicht geöffnet oder beschrieben werden. "
               "Prüfe das Datenverzeichnis und deine Zugriffsrechte."));
  }
  if (code == 13) {
    throw StorageError(QCoreApplication::translate("App",
                                                   "Es ist nicht genügend Speicherplatz verfügbar. "
                                                   "Schaffe Platz und versuche es erneut."));
  }
  throw StorageError(QCoreApplication::translate(
                         "App", "Der Datenbankzugriff ist fehlgeschlagen. "
                                "Prüfe Datenträger und Zugriffsrechte und versuche es erneut.\n%1")
                         .arg(error.text()));
}

void execute(QSqlQuery& query, const QString& sql) {
  if (!query.exec(sql)) {
    fail(query.lastError());
  }
}

void execute(QSqlQuery& query) {
  if (!query.exec()) {
    fail(query.lastError());
  }
}

void prepare(QSqlQuery& query, const QString& sql) {
  if (!query.prepare(sql)) {
    fail(query.lastError());
  }
}

class Transaction final {
public:
  explicit Transaction(QSqlDatabase& database) : database_(database) {
    if (!database_.transaction()) {
      fail(database_.lastError());
    }
  }
  ~Transaction() {
    if (!committed_) {
      database_.rollback();
    }
  }
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  void commit() {
    if (!database_.commit()) {
      fail(database_.lastError());
    }
    committed_ = true;
  }

private:
  QSqlDatabase& database_;
  bool committed_ = false;
};

void bindDraft(QSqlQuery& query, const ItemDraft& draft) {
  // Empty optional QString values must be TEXT, never SQL NULL.
  query.bindValue(QStringLiteral(":name"), draft.name.isNull() ? QStringLiteral("") : draft.name);
  query.bindValue(QStringLiteral(":maker"),
                  draft.maker.isNull() ? QStringLiteral("") : draft.maker);
  query.bindValue(QStringLiteral(":location"),
                  draft.location.isNull() ? QStringLiteral("") : draft.location);
  query.bindValue(QStringLiteral(":note"), draft.note.isNull() ? QStringLiteral("") : draft.note);
  query.bindValue(QStringLiteral(":photo"), draft.photo.isEmpty()
                                                ? QVariant(QMetaType::fromType<QByteArray>())
                                                : QVariant(draft.photo));
}

void requireAffectedRow(const QSqlQuery& query) {
  if (query.numRowsAffected() != 1) {
    throw StorageError(QCoreApplication::translate(
        "App", "Dieses Sammlerstück ist nicht mehr vorhanden. "
               "Lade die Sammlung erneut. Deine Eingaben bleiben erhalten."));
  }
}

} // namespace

SqliteItemRepository::SqliteItemRepository(QString directory) : directory_(std::move(directory)) {}

SqliteItemRepository::~SqliteItemRepository() { close(); }

void SqliteItemRepository::close() {
  initialized_ = false;
  if (database_.isValid()) {
    database_.close();
    database_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName_);
  }
  lock_.reset();
}

void SqliteItemRepository::initialize() {
  if (initialized_) {
    return;
  }
  close();
  if (!QDir::isAbsolutePath(directory_) || !QDir().mkpath(directory_)) {
    throw StorageError(
        QCoreApplication::translate("App", "Das Datenverzeichnis kann nicht angelegt werden. "
                                           "Prüfe Pfad, Datenträger und Zugriffsrechte."));
  }
  directory_ = QFileInfo(directory_).canonicalFilePath();
  lock_ = std::make_unique<QLockFile>(QDir(directory_).filePath(QStringLiteral("collection.lock")));
  lock_->setStaleLockTime(0);
  if (!lock_->tryLock()) {
    const bool otherInstance = lock_->error() == QLockFile::LockFailedError;
    lock_.reset();
    throw StorageError(
        otherInstance
            ? QCoreApplication::translate(
                  "App", "Diese Sammlung ist bereits in einer anderen App-Instanz geöffnet. "
                         "Schließe sie dort und lade hier erneut.")
            : QCoreApplication::translate(
                  "App",
                  "Das Datenverzeichnis ist nicht beschreibbar. Prüfe deine Zugriffsrechte."));
  }
  try {
    const QString path = QDir(directory_).filePath(QStringLiteral("sammlervitrine.sqlite3"));
    const bool isNew = !QFileInfo::exists(path);
    connectionName_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(path);
    database_.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=2000"));
    if (!database_.open()) {
      fail(database_.lastError());
    }
    QSqlQuery query(database_);
    execute(query, QStringLiteral("PRAGMA user_version"));
    if (!query.next()) {
      throw StorageError(
          QCoreApplication::translate("App", "Die Schema-Version konnte nicht gelesen werden."));
    }
    const int version = query.value(0).toInt();
    query.finish();
    if (version > 1) {
      throw StorageError(QCoreApplication::translate(
          "App", "Diese Datenbank benötigt eine neuere Version von SammlerVitrine. "
                 "Die vorhandene Datei wird nicht verändert."));
    }
    if (isNew) {
      Transaction transaction(database_);
      execute(query,
              QStringLiteral(
                  "CREATE TABLE items (id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "name TEXT NOT NULL, maker TEXT NOT NULL DEFAULT '', "
                  "location TEXT NOT NULL DEFAULT '', note TEXT NOT NULL DEFAULT '', photo BLOB)"));
      execute(query, QStringLiteral("PRAGMA user_version=1"));
      execute(query, QStringLiteral("PRAGMA application_id=1398166612"));
      query.finish();
      transaction.commit();
    } else if (version != 1) {
      throw StorageError(QCoreApplication::translate(
          "App", "Die vorhandene Datei hat kein bekanntes SammlerVitrine-Schema. "
                 "Sie wird nicht überschrieben."));
    }
    execute(query, QStringLiteral("PRAGMA application_id"));
    if (!query.next() || query.value(0).toInt() != 1398166612) {
      throw StorageError(QCoreApplication::translate(
          "App", "Diese Datenbank gehört nicht zu SammlerVitrine. Sie wird nicht verändert."));
    }
    query.finish();
    execute(query, QStringLiteral("PRAGMA quick_check"));
    if (!query.next() || query.value(0).toString() != QStringLiteral("ok")) {
      throw StorageError(
          QCoreApplication::translate("App", "Die Datenbankprüfung hat einen Schaden gefunden. "
                                             "Sichere die Datei. Sie wird nicht ersetzt."));
    }
    query.finish();
    execute(query, QStringLiteral("PRAGMA table_info(items)"));
    QStringList columns;
    bool validTypes = true;
    while (query.next()) {
      columns.append(query.value(1).toString());
      const bool isId = columns.size() == 1;
      const bool isPhoto = query.value(1).toString() == QStringLiteral("photo");
      validTypes = validTypes &&
                   query.value(2).toString().toUpper() == (isId      ? QStringLiteral("INTEGER")
                                                           : isPhoto ? QStringLiteral("BLOB")
                                                                     : QStringLiteral("TEXT")) &&
                   query.value(5).toInt() == (isId ? 1 : 0) &&
                   (isId || isPhoto || query.value(3).toBool());
    }
    if (!validTypes || columns != QStringList{QStringLiteral("id"), QStringLiteral("name"),
                                              QStringLiteral("maker"), QStringLiteral("location"),
                                              QStringLiteral("note"), QStringLiteral("photo")}) {
      throw StorageError(
          QCoreApplication::translate("App", "Die Datenbankstruktur ist unbekannt oder beschädigt. "
                                             "Die Datei wird nicht ersetzt."));
    }
    initialized_ = true;
  } catch (...) {
    close();
    throw;
  }
}

ItemList SqliteItemRepository::findAll() {
  QSqlQuery query(database_);
  execute(query,
          QStringLiteral(
              "SELECT id,name,maker,location,note,photo IS NOT NULL FROM items ORDER BY id"));
  ItemList items;
  while (query.next()) {
    items.append({query.value(0).toLongLong(),
                  {query.value(1).toString(), query.value(2).toString(), query.value(3).toString(),
                   query.value(4).toString()},
                  query.value(5).toBool()});
  }
  if (query.lastError().isValid()) {
    fail(query.lastError());
  }
  return items;
}

Collectible SqliteItemRepository::find(qint64 id) {
  QSqlQuery query(database_);
  // Limit even externally injected BLOBs before Qt materializes their bytes.
  prepare(query, QStringLiteral("SELECT id,name,maker,location,note,length(photo),typeof(photo),"
                                "CASE WHEN length(photo)<=10485760 THEN photo ELSE NULL END "
                                "FROM items WHERE id=:id"));
  query.bindValue(QStringLiteral(":id"), id);
  execute(query);
  if (!query.next()) {
    if (query.lastError().isValid()) {
      fail(query.lastError());
    }
    throw StorageError(QCoreApplication::translate(
        "App", "Dieses Sammlerstück ist nicht mehr vorhanden. Bitte lade die Sammlung erneut."));
  }
  const bool hasPhoto = query.value(6).toString() != QStringLiteral("null");
  if (hasPhoto &&
      (query.value(6).toString() != QStringLiteral("blob") || query.value(5).toLongLong() <= 0 ||
       query.value(5).toLongLong() > maxPhotoBytes)) {
    throw StorageError(
        QCoreApplication::translate("App", "Das gespeicherte Foto ist ungültig oder zu groß. Der "
                                           "Eintrag wird zum Schutz des Fotos nicht bearbeitet."));
  }
  return {query.value(0).toLongLong(),
          {query.value(1).toString(), query.value(2).toString(), query.value(3).toString(),
           query.value(4).toString(), query.value(7).toByteArray()},
          hasPhoto};
}

Collectible SqliteItemRepository::create(const ItemDraft& draft) {
  const auto valid = validateDraft(draft);
  Transaction transaction(database_);
  QSqlQuery query(database_);
  prepare(query, QStringLiteral("INSERT INTO items(name,maker,location,note,photo) "
                                "VALUES(:name,:maker,:location,:note,:photo)"));
  bindDraft(query, valid);
  execute(query);
  const auto id = query.lastInsertId().toLongLong();
  query.finish();
  transaction.commit();
  return {id, valid, !valid.photo.isEmpty()};
}

Collectible SqliteItemRepository::update(qint64 id, const ItemDraft& draft) {
  const auto valid = validateDraft(draft);
  Transaction transaction(database_);
  QSqlQuery query(database_);
  prepare(query, QStringLiteral("UPDATE items SET name=:name,maker=:maker,location=:location,"
                                "note=:note,photo=:photo WHERE id=:id"));
  bindDraft(query, valid);
  query.bindValue(QStringLiteral(":id"), id);
  execute(query);
  requireAffectedRow(query);
  query.finish();
  transaction.commit();
  return {id, valid, !valid.photo.isEmpty()};
}

void SqliteItemRepository::remove(qint64 id) {
  Transaction transaction(database_);
  QSqlQuery query(database_);
  prepare(query, QStringLiteral("DELETE FROM items WHERE id=:id"));
  query.bindValue(QStringLiteral(":id"), id);
  execute(query);
  requireAffectedRow(query);
  query.finish();
  transaction.commit();
}

} // namespace vitrine
