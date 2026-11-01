#include "application/collection_codec.h"
#include "application/photo_importer.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
namespace vitrine {
namespace {
[[noreturn]] void invalid() {
  throw TransferError(
      QCoreApplication::translate("App", "Die Sammlungsdatei enthält ungültige oder zu große "
                                         "Daten. Die Sammlung bleibt unverändert."));
}
void check(const ItemDraft& draft) {
  if (validateDraft(draft) != draft)
    invalid();
  for (const auto& text : {draft.name, draft.maker, draft.location, draft.note}) {
    if (!text.isValidUtf16() || text.size() > 1000000)
      invalid();
  }
  if (!draft.photo.isEmpty())
    (void)decodePhoto(draft.photo);
}
} // namespace
QJsonObject encodeEntry(const ItemDraft& draft) {
  check(draft);
  QJsonObject object{{QStringLiteral("name"), draft.name},
                     {QStringLiteral("maker"), draft.maker},
                     {QStringLiteral("location"), draft.location},
                     {QStringLiteral("note"), draft.note},
                     {QStringLiteral("photoBase64"),
                      draft.photo.isEmpty()
                          ? QJsonValue(QJsonValue::Null)
                          : QJsonValue(QString::fromLatin1(draft.photo.toBase64()))}};
  return object;
}
ItemDraft decodeEntry(const QJsonObject& object) {
  if (object.size() != 5)
    invalid();
  for (const auto& field : {QStringLiteral("name"), QStringLiteral("maker"),
                            QStringLiteral("location"), QStringLiteral("note")})
    if (!object.value(field).isString())
      invalid();
  ItemDraft draft;
  draft.name = object.value(QStringLiteral("name")).toString();
  draft.maker = object.value(QStringLiteral("maker")).toString();
  draft.location = object.value(QStringLiteral("location")).toString();
  draft.note = object.value(QStringLiteral("note")).toString();
  const auto photo = object.value(QStringLiteral("photoBase64"));
  if (!photo.isNull()) {
    if (!photo.isString())
      invalid();
    const auto encoded = photo.toString().toLatin1();
    if (encoded.isEmpty() || encoded.size() > ((maxPhotoBytes + 2) / 3) * 4)
      invalid();
    const auto decoded =
        QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.toBase64() != encoded)
      invalid();
    draft.photo = decoded.decoded;
  }
  check(draft);
  return draft;
}
QByteArray entryFingerprint(const ItemDraft& draft) {
  auto object = encodeEntry(draft);
  return QCryptographicHash::hash(QJsonDocument(object).toJson(QJsonDocument::Compact),
                                  QCryptographicHash::Sha256);
}
} // namespace vitrine
