#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <stdexcept>

namespace vitrine {

struct ItemDraft {
  QString name;
  QString maker;
  QString location;
  QString note;
  QByteArray photo{}; // Empty means no photo. Original encoded bytes otherwise.
  QString color = {}; // Optional #RRGGBB or #RRGGBBAA.
  bool operator==(const ItemDraft&) const = default;
};

struct Collectible {
  qint64 id = 0;
  ItemDraft fields;
  bool hasPhoto = false; // List queries do not load fields.photo.
  bool operator==(const Collectible&) const = default;
};

using ItemList = QVector<Collectible>;

class ValidationError final : public std::runtime_error {
public:
  explicit ValidationError(const QString& message) : std::runtime_error(message.toStdString()) {}
};

ItemDraft validateDraft(ItemDraft draft);
QString searchKey(const QString& text);
enum class SortField : quint8 { Name, Details, Color, Location, Photo };
using TransferDraftList = QVector<ItemDraft>;
ItemList searchItems(const ItemList& items, const QString& query, SortField field = SortField::Name,
                     Qt::SortOrder order = Qt::AscendingOrder);

} // namespace vitrine

Q_DECLARE_METATYPE(vitrine::ItemDraft)
Q_DECLARE_METATYPE(vitrine::Collectible)
Q_DECLARE_METATYPE(vitrine::ItemList)
