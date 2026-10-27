#include "persistence/data_directory.h"
#include "persistence/sqlite_item_repository.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QTranslator>

int main(int argc, char* argv[]) {
#ifdef Q_OS_LINUX
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME")) {
    qputenv("QT_QPA_PLATFORMTHEME", "gtk3");
  }
#endif
  QApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("de.appfabrik.sammlervitrine"));
  QCoreApplication::setOrganizationDomain(QStringLiteral("appfabrik.de"));
  QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
  QApplication::setApplicationDisplayName(QStringLiteral("SammlerVitrine"));
  QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/branding/icon-256.png")));
  QTranslator translator;
  if (translator.load(QStringLiteral("qtbase_de"),
                      QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
    application.installTranslator(&translator);
  }
  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("Eine lokale Sammlung für deine Sammlerstücke."));
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption dataOption(QStringLiteral("data-dir"),
                                QStringLiteral("Isoliertes absolutes Datenverzeichnis."),
                                QStringLiteral("pfad"));
  parser.addOption(dataOption);
  parser.process(application);
  try {
    const auto directory =
        vitrine::resolveDataDirectory(parser.value(dataOption), parser.isSet(dataOption));
    vitrine::StorageController controller(
        [directory] { return std::make_unique<vitrine::SqliteItemRepository>(directory); });
    vitrine::MainWindow window(controller, directory);
    window.show();
    return QApplication::exec();
  } catch (const std::exception& error) {
    QMessageBox::critical(nullptr, QStringLiteral("SammlerVitrine kann nicht starten"),
                          QString::fromUtf8(error.what()));
    return 1;
  }
}
