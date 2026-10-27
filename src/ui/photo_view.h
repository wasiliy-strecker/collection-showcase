#pragma once
#include <QImage>
#include <QWidget>

namespace vitrine {
class PhotoView final : public QWidget {
  Q_OBJECT
public:
  explicit PhotoView(QWidget* parent = nullptr);
  void showImage(const QImage& image);
  void showMessage(const QString& message);
  const QImage& image() const { return image_; }
  QString message() const { return message_; }

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  QImage image_;
  QString message_;
};
} // namespace vitrine
