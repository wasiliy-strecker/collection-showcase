#include "ui/photo_view.h"
#include <QPainter>

namespace vitrine {
PhotoView::PhotoView(QWidget* parent) : QWidget(parent) {
  setMinimumSize(160, 120);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAccessibleName(QStringLiteral("Foto des Sammlerstücks"));
  showMessage(QStringLiteral("Kein Foto"));
}
void PhotoView::showImage(const QImage& image) {
  image_ = image;
  message_.clear();
  setAccessibleDescription(QStringLiteral("Gespeichertes Foto"));
  update();
}
void PhotoView::showMessage(const QString& message) {
  image_ = {};
  message_ = message;
  setAccessibleDescription(message);
  update();
}
void PhotoView::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.fillRect(rect(), palette().alternateBase());
  if (image_.isNull()) {
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(rect().adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap,
                     message_);
    return;
  }
  const auto size = image_.size().scaled(this->size(), Qt::KeepAspectRatio);
  const QRect target(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.drawImage(target, image_);
}
} // namespace vitrine
