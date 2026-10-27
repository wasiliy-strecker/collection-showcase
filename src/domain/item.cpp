#include "domain/item.h"

#include <QCollator>
#include <QLocale>
#include <algorithm>

namespace vitrine {

ItemDraft validateDraft(ItemDraft draft) {
  draft.name = draft.name.trimmed();
  draft.maker = draft.maker.trimmed();
  draft.location = draft.location.trimmed();
  if (draft.name.isEmpty()) {
    throw ValidationError(QStringLiteral("Bitte gib eine Bezeichnung ein."));
  }
  return draft;
}

QString searchKey(const QString& text) {
  return text.normalized(QString::NormalizationForm_C)
      .toCaseFolded()
      .normalized(QString::NormalizationForm_C);
}

ItemList searchItems(const ItemList& items, const QString& query) {
  const auto needle = searchKey(query.trimmed());
  ItemList matches;
  for (const auto& item : items) {
    const auto& f = item.fields;
    if (searchKey(f.name).contains(needle) || searchKey(f.maker).contains(needle) ||
        searchKey(f.location).contains(needle) || searchKey(f.note).contains(needle)) {
      matches.append(item);
    }
  }
  QCollator collator{QLocale(QLocale::German, QLocale::Germany)};
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  collator.setNumericMode(true);
  std::sort(matches.begin(), matches.end(),
            [&collator](const Collectible& left, const Collectible& right) {
              const int comparison =
                  collator.compare(left.fields.name.normalized(QString::NormalizationForm_C),
                                   right.fields.name.normalized(QString::NormalizationForm_C));
              return comparison == 0 ? left.id < right.id : comparison < 0;
            });
  return matches;
}

} // namespace vitrine
