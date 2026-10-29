#pragma once

#include "application/item_service.h"

#include <QImage>
#include <QObject>
#include <QThread>
#include <functional>
#include <memory>

namespace vitrine {

enum class Operation : quint8 { Load, Save, Remove, Language };
struct ItemRequest {
  qint64 id;
  quint64 token;
};
using RepositoryFactory = std::function<std::unique_ptr<ItemRepository>()>;

class StorageWorker final : public QObject {
  Q_OBJECT
public:
  explicit StorageWorker(RepositoryFactory factory);

public slots:
  void load();
  void save(qint64 id, const vitrine::ItemDraft& draft);
  void remove(qint64 id);
  void saveLanguage(const QString& directory, const QString& preference);
  void readItem(const vitrine::ItemRequest& request);
  void importFile(const QString& path, quint64 token);

signals:
  void loaded(const vitrine::ItemList& items);
  void itemRead(quint64 token, const vitrine::Collectible& item, const QImage& preview);
  void readFailed(quint64 token, const QString& message);
  void imported(quint64 token, const QByteArray& bytes, const QImage& preview);
  void importFailed(quint64 token, const QString& message);
  void saved(const vitrine::Collectible& item);
  void removed(qint64 id);
  void languageSaved(const QString& preference);
  void failed(vitrine::Operation operation, const QString& message);

private:
  RepositoryFactory factory_;
  std::unique_ptr<ItemRepository> repository_;
};

class StorageController final : public QObject {
  Q_OBJECT
public:
  explicit StorageController(RepositoryFactory factory, QObject* parent = nullptr);
  ~StorageController() override;
  bool isBusy() const { return busy_; }
  bool load();
  bool save(qint64 id, const ItemDraft& draft);
  bool remove(qint64 id);
  bool saveLanguage(const QString& directory, const QString& preference);
  void readItem(const vitrine::ItemRequest& request);
  bool importFile(const QString& path, quint64 token);

signals:
  void loaded(const vitrine::ItemList& items);
  void itemRead(quint64 token, const vitrine::Collectible& item, const QImage& preview);
  void readFailed(quint64 token, const QString& message);
  void imported(quint64 token, const QByteArray& bytes, const QImage& preview);
  void importFailed(quint64 token, const QString& message);
  void saved(const vitrine::Collectible& item);
  void removed(qint64 id);
  void languageSaved(const QString& preference);
  void failed(vitrine::Operation operation, const QString& message);
  void busyChanged(bool busy);
  void loadRequested();
  void readRequested(const vitrine::ItemRequest& request);
  void importRequested(const QString& path, quint64 token);
  void saveRequested(qint64 id, const vitrine::ItemDraft& draft);
  void removeRequested(qint64 id);
  void languageRequested(const QString& directory, const QString& preference);

private:
  bool begin();
  void finish();
  QThread thread_;
  bool busy_ = false;
};

} // namespace vitrine
Q_DECLARE_METATYPE(vitrine::Operation)
Q_DECLARE_METATYPE(vitrine::ItemRequest)
