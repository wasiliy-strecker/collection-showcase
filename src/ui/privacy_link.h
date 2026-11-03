#pragma once

#include <QUrl>
#include <functional>

class QWidget;

namespace vitrine {
using BrowserOpener = std::function<bool(const QUrl&)>;
QUrl privacyPolicyUrl();
bool openInDefaultBrowser(const QUrl& url);
void openPrivacyPolicy(QWidget* parent, const BrowserOpener& browser);
} // namespace vitrine
