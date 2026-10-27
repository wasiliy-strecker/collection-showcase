#pragma once

#include "ui/photo_view.h"
#include "ui/storage_controller.h"

#include <QDialog>
#include <optional>

class QCloseEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace vitrine {

class ItemEditor final : public QDialog {
  Q_OBJECT
public:
  ItemEditor(StorageController& controller, std::optional<Collectible> existing,
             QWidget* parent = nullptr, const QImage& preview = {});
  ItemDraft draft() const;
  bool requestClose();

public slots:
  void reject() override;
  void save();
  void importPath(const QString& path);
  void removePhoto();

protected:
  void closeEvent(QCloseEvent* event) override;

private:
  void setSaving(bool saving);
  StorageController& controller_;
  qint64 id_ = 0;
  ItemDraft original_;
  QLineEdit* name_;
  QLineEdit* maker_;
  QLineEdit* location_;
  QPlainTextEdit* note_;
  QLabel* error_;
  QPushButton* save_;
  QPushButton* cancel_;
  PhotoView* photoView_;
  QPushButton* choosePhoto_;
  QPushButton* removePhoto_;
  QByteArray photo_;
  quint64 importToken_ = 0;
  bool importing_ = false;
  bool choosing_ = false;
  bool saving_ = false;
  bool validationError_ = false;
};

} // namespace vitrine
