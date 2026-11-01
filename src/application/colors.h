#pragma once
#include <QColor>
#include <QImage>
namespace vitrine {
QColor parseColor(const QString& value);
QString encodeColor(const QColor& color);
QImage colorSwatch(const QString& value, int size = 24);
} // namespace vitrine
