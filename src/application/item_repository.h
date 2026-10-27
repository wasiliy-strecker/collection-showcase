#pragma once

#include "domain/item.h"

namespace vitrine {

class StorageError final : public std::runtime_error {
public:
  explicit StorageError(const QString& message) : std::runtime_error(message.toStdString()) {}
};

// Implementations belong to one thread. Every successful mutation is committed.
class ItemRepository {
public:
  virtual ~ItemRepository() = default;
  virtual void initialize() = 0;
  virtual ItemList findAll() = 0;
  virtual Collectible find(qint64 id) = 0;
  virtual Collectible create(const ItemDraft& draft) = 0;
  virtual Collectible update(qint64 id, const ItemDraft& draft) = 0;
  virtual void remove(qint64 id) = 0;
};

} // namespace vitrine
