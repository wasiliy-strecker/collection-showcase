#include "domain/item.h"
#include <QCoreApplication>

#include <QCollator>
#include <QLocale>
#include <QRegularExpression>
#include <algorithm>

namespace vitrine {

ItemDraft validateDraft(ItemDraft draft) {
  static const QRegularExpression colorPattern(
      QStringLiteral("\\A#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?\\z"));
  if (!draft.color.isEmpty() && !colorPattern.match(draft.color).hasMatch())
    throw ValidationError(QCoreApplication::translate(
        "App", "Die Farbe muss ein Hexwert wie #RRGGBB oder #RRGGBBAA sein."));
  draft.name = draft.name.trimmed();
  draft.maker = draft.maker.trimmed();
  draft.location = draft.location.trimmed();
  if (draft.name.isEmpty()) {
    throw ValidationError(QCoreApplication::translate("App", "Bitte gib eine Bezeichnung ein."));
  }
  return draft;
}

QString searchKey(const QString& text) {
  return text.normalized(QString::NormalizationForm_C)
      .toCaseFolded()
      .normalized(QString::NormalizationForm_C);
}

ItemList searchItems(const ItemList& items, const QString& query, SortField field,
                     Qt::SortOrder order) {
  const auto needle = searchKey(query.trimmed());
  ItemList matches;
  for (const auto& item : items) {
    const auto& f = item.fields;
    if (searchKey(f.name).contains(needle) || searchKey(f.maker).contains(needle) ||
        searchKey(f.location).contains(needle) || searchKey(f.note).contains(needle) ||
        searchKey(f.color).contains(needle)) {
      matches.append(item);
    }
  }
  QCollator collator{QLocale()};
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  collator.setNumericMode(true);
  std::sort(
      matches.begin(), matches.end(),
      [&collator, field, order](const Collectible& left, const Collectible& right) {
        int comparison = 0;
        switch (field) {
        case SortField::Name:
          comparison = collator.compare(left.fields.name.normalized(QString::NormalizationForm_C),
                                        right.fields.name.normalized(QString::NormalizationForm_C));
          break;
        case SortField::Details:
          comparison =
              collator.compare(left.fields.maker.normalized(QString::NormalizationForm_C),
                               right.fields.maker.normalized(QString::NormalizationForm_C));
          break;
        case SortField::Color:
          comparison = collator.compare(left.fields.color, right.fields.color);
          break;
        case SortField::Location:
          comparison =
              collator.compare(left.fields.location.normalized(QString::NormalizationForm_C),
                               right.fields.location.normalized(QString::NormalizationForm_C));
          break;
        case SortField::Photo:
          comparison = static_cast<int>(left.hasPhoto) - static_cast<int>(right.hasPhoto);
          break;
        }
        return comparison == 0 ? left.id < right.id
                               : (order == Qt::AscendingOrder ? comparison < 0 : comparison > 0);
      });
  return matches;
}

} // namespace vitrine
