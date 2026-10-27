#pragma once

#include <QString>

namespace vitrine {
QString defaultDataDirectory();
QString resolveDataDirectory(const QString& overridePath, bool explicitlyProvided);
} // namespace vitrine
