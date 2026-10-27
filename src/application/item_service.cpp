#include "application/item_service.h"

namespace vitrine {

ItemList ItemService::load() {
  repository_.initialize();
  return repository_.findAll();
}

Collectible ItemService::save(qint64 id, const ItemDraft& draft) {
  const auto validated = validateDraft(draft);
  return id == 0 ? repository_.create(validated) : repository_.update(id, validated);
}

void ItemService::remove(qint64 id) { repository_.remove(id); }

} // namespace vitrine
