#include "ui/storage_controller.h"
#include "application/photo_importer.h"
#include "persistence/language_preferences.h"
#include <QCoreApplication>

#include <utility>

namespace vitrine {

StorageWorker::StorageWorker(RepositoryFactory factory) : factory_(std::move(factory)) {}

void StorageWorker::load() {
  try {
    if (!repository_) {
      repository_ = factory_();
    }
    ItemService service(*repository_);
    emit loaded(service.load());
  } catch (const std::exception& error) {
    emit failed(Operation::Load, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::save(qint64 id, const ItemDraft& draft) {
  try {
    if (!repository_) {
      throw StorageError(
          QCoreApplication::translate("App", "Die Sammlung ist noch nicht geladen."));
    }
    ItemService service(*repository_);
    emit saved(service.save(id, draft));
  } catch (const std::exception& error) {
    emit failed(Operation::Save, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::remove(qint64 id) {
  try {
    if (!repository_) {
      throw StorageError(
          QCoreApplication::translate("App", "Die Sammlung ist noch nicht geladen."));
    }
    ItemService service(*repository_);
    service.remove(id);
    emit removed(id);
  } catch (const std::exception& error) {
    emit failed(Operation::Remove, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::readItem(const ItemRequest& request) {
  try {
    if (!repository_) {
      throw StorageError(
          QCoreApplication::translate("App", "Die Sammlung ist noch nicht geladen."));
    }
    const auto item = repository_->find(request.id);
    const auto preview = item.hasPhoto ? decodePhoto(item.fields.photo).preview : QImage();
    emit itemRead(request.token, item, preview);
  } catch (const std::exception& error) {
    emit readFailed(request.token, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::importFile(const QString& path, quint64 token) {
  try {
    const auto photo = importPhoto(path);
    emit imported(token, photo.bytes, photo.preview);
  } catch (const std::exception& error) {
    emit importFailed(token, QString::fromUtf8(error.what()));
  }
}

void StorageWorker::saveLanguage(const QString& directory, const QString& preference) {
  try {
    saveLanguagePreference(directory, preference);
    emit languageSaved(preference);
  } catch (const std::exception& error) {
    emit failed(Operation::Language, QString::fromUtf8(error.what()));
  }
}

StorageController::StorageController(RepositoryFactory factory, QObject* parent) : QObject(parent) {
  qRegisterMetaType<ItemDraft>();
  qRegisterMetaType<Collectible>();
  qRegisterMetaType<ItemList>();
  qRegisterMetaType<Operation>();
  qRegisterMetaType<ItemRequest>();
  auto* worker = new StorageWorker(std::move(factory));
  worker->moveToThread(&thread_);
  connect(&thread_, &QThread::finished, worker, &QObject::deleteLater);
  connect(this, &StorageController::loadRequested, worker, &StorageWorker::load);
  connect(this, &StorageController::saveRequested, worker, &StorageWorker::save);
  connect(this, &StorageController::removeRequested, worker, &StorageWorker::remove);
  connect(this, &StorageController::languageRequested, worker, &StorageWorker::saveLanguage);
  connect(worker, &StorageWorker::languageSaved, this, [this](const QString& preference) {
    finish();
    emit languageSaved(preference);
  });
  connect(worker, &StorageWorker::loaded, this, [this](const ItemList& items) {
    finish();
    emit loaded(items);
  });
  connect(worker, &StorageWorker::saved, this, [this](const Collectible& item) {
    finish();
    emit saved(item);
  });
  connect(worker, &StorageWorker::removed, this, [this](qint64 id) {
    finish();
    emit removed(id);
  });
  connect(worker, &StorageWorker::failed, this,
          [this](Operation operation, const QString& message) {
            finish();
            emit failed(operation, message);
          });
  connect(this, &StorageController::readRequested, worker, &StorageWorker::readItem);
  connect(this, &StorageController::importRequested, worker, &StorageWorker::importFile);
  connect(worker, &StorageWorker::itemRead, this, &StorageController::itemRead);
  connect(worker, &StorageWorker::readFailed, this, &StorageController::readFailed);
  connect(worker, &StorageWorker::imported, this,
          [this](quint64 token, const QByteArray& bytes, const QImage& preview) {
            finish();
            emit imported(token, bytes, preview);
          });
  connect(worker, &StorageWorker::importFailed, this,
          [this](quint64 token, const QString& message) {
            finish();
            emit importFailed(token, message);
          });
  thread_.start();
}

StorageController::~StorageController() {
  // MainWindow prevents normal closure during outstanding work. This also makes
  // exceptional teardown safe. All connection ownership stays in the worker.
  thread_.quit();
  thread_.wait();
}

bool StorageController::begin() {
  if (busy_) {
    return false;
  }
  busy_ = true;
  emit busyChanged(true);
  return true;
}

void StorageController::finish() {
  busy_ = false;
  emit busyChanged(false);
}

bool StorageController::load() {
  if (!begin()) {
    return false;
  }
  emit loadRequested();
  return true;
}

bool StorageController::save(qint64 id, const ItemDraft& draft) {
  if (!begin()) {
    return false;
  }
  emit saveRequested(id, draft);
  return true;
}

bool StorageController::remove(qint64 id) {
  if (!begin()) {
    return false;
  }
  emit removeRequested(id);
  return true;
}

void StorageController::readItem(const ItemRequest& request) { emit readRequested(request); }
bool StorageController::importFile(const QString& path, quint64 token) {
  if (!begin()) {
    return false;
  }
  emit importRequested(path, token);
  return true;
}
bool StorageController::saveLanguage(const QString& directory, const QString& preference) {
  if (!begin()) {
    return false;
  }
  emit languageRequested(directory, preference);
  return true;
}

} // namespace vitrine
