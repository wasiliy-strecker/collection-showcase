#pragma once

#include "application/item_repository.h"

namespace vitrine {

class ItemService final {
public:
  explicit ItemService(ItemRepository& repository) : repository_(repository) {}
  ItemList load();
  Collectible save(qint64 id, const ItemDraft& draft);
  void remove(qint64 id);

private:
  ItemRepository& repository_;
};

} // namespace vitrine
