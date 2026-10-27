#pragma once

#include <QWidget>

namespace vitrine {
bool confirmDiscard(QWidget* parent);
bool confirmDelete(QWidget* parent, const QString& name);
} // namespace vitrine
