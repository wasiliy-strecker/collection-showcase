#pragma once

#include "domain/item.h"

namespace vitrine {

class StorageError final : public std::runtime_error {
public:
  explicit StorageError(const QString& message) : std::runtime_error(message.toStdString()) {}
};

struct CollectionImportResult {
  qsizetype added = 0;
  qsizetype skipped = 0;
  ItemList entries;
};

// Implementations belong to one thread. Every successful mutation is committed.
class ItemRepository {
public:
  virtual ~ItemRepository() = default;
  virtual TransferDraftList transferSnapshot() = 0;
  virtual CollectionImportResult importEntries(const TransferDraftList& drafts) = 0;
  virtual void initialize() = 0;
  virtual ItemList findAll() = 0;
  virtual Collectible find(qint64 id) = 0;
  virtual Collectible create(const ItemDraft& draft) = 0;
  virtual Collectible update(qint64 id, const ItemDraft& draft) = 0;
  virtual void remove(qint64 id) = 0;
};

} // namespace vitrine

Q_DECLARE_METATYPE(vitrine::CollectionImportResult)
