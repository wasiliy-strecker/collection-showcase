#pragma once

#include "ui/item_editor.h"

#include <QMainWindow>
#include <QPointer>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace vitrine {

class MainWindow final : public QMainWindow {
  Q_OBJECT
public:
  MainWindow(StorageController& controller, QString dataDirectory,
             QString languagePreference = QStringLiteral("auto"), QWidget* parent = nullptr);

protected:
  void closeEvent(QCloseEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void refresh(std::optional<qint64> preferredId = std::nullopt, int fallbackRow = 0);
  void updateSelection();
  void updateActions();
  std::optional<Collectible> selectedItem() const;
  void openEditor(bool editing);
  void showEditor(std::optional<Collectible> item, const QImage& preview = {});
  void deleteSelected();
  bool beginTransfer();
  void finishTransfer(const QString& message);
  void exportCollection();
  void importCollection();
  void showAbout();
  void showLanguage();
  StorageController& controller_;
  QString dataDirectory_;
  QString languagePreference_;
  ItemList items_;
  ItemList visibleItems_;
  QPointer<ItemEditor> editor_;
  QLineEdit* search_;
  QTableWidget* table_;
  QPlainTextEdit* note_;
  QLabel* detailTitle_;
  QLabel* state_;
  QLabel* feedback_;
  QLabel* count_;
  QPushButton* add_;
  QPushButton* edit_;
  QPushButton* remove_;
  QPushButton* reload_;
  QPushButton* language_;
  PhotoView* photoView_;
  quint64 readToken_ = 0;
  bool editLoading_ = false;
  QPushButton* exportCollection_;
  QPushButton* importCollection_;
  QString transferDirectory_;
  bool transferRunning_ = false;
  bool ready_ = false;
};

} // namespace vitrine
