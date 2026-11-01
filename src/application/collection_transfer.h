#pragma once

#include "application/collection_file.h"
#include "application/item_repository.h"

namespace vitrine {

class CollectionTransfer final {
public:
  CollectionTransfer(ItemRepository& repository, CollectionFile& files)
      : repository_(repository), files_(files) {}
  qsizetype exportEntries(const CollectionExportTarget& target, bool replacementApproved);

private:
  ItemRepository& repository_;
  CollectionFile& files_;
};

} // namespace vitrine
