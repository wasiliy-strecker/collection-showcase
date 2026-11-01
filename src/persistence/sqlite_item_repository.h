#pragma once

#include "application/item_repository.h"

#include <QLockFile>
#include <QSqlDatabase>
#include <memory>

namespace vitrine {

class SqliteItemRepository final : public ItemRepository {
public:
  explicit SqliteItemRepository(QString directory);
  ~SqliteItemRepository() override;
  SqliteItemRepository(const SqliteItemRepository&) = delete;
  SqliteItemRepository& operator=(const SqliteItemRepository&) = delete;
  TransferDraftList transferSnapshot() override;
  CollectionImportResult importEntries(const TransferDraftList& drafts) override;
  void initialize() override;
  ItemList findAll() override;
  Collectible find(qint64 id) override;
  Collectible create(const ItemDraft& draft) override;
  Collectible update(qint64 id, const ItemDraft& draft) override;
  void remove(qint64 id) override;

private:
  ItemList findAllInTransaction();
  Collectible findInTransaction(qint64 id);
  Collectible createInTransaction(const ItemDraft& draft);
  void close();
  QString directory_;
  QString connectionName_;
  QSqlDatabase database_;
  std::unique_ptr<QLockFile> lock_;
  bool initialized_ = false;
};

} // namespace vitrine
