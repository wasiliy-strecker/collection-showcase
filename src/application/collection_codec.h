#pragma once
#include "application/collection_file.h"
#include <QJsonObject>
namespace vitrine {
QJsonObject encodeEntry(const ItemDraft& draft);
ItemDraft decodeEntry(const QJsonObject& object);
QByteArray entryFingerprint(const ItemDraft& draft);
} // namespace vitrine
