#include "ui/item_editor.h"
#include "ui/ui_support.h"
#include <QCoreApplication>

#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>
#include <atomic>

namespace vitrine {

ItemEditor::ItemEditor(StorageController& controller, std::optional<Collectible> existing,
                       QWidget* parent, const QImage& preview)
    : QDialog(parent), controller_(controller),
      color_(new ColorField(existing ? existing->fields.color : QString{}, this)),
      name_(new QLineEdit(this)), maker_(new QLineEdit(this)), location_(new QLineEdit(this)),
      note_(new QPlainTextEdit(this)), error_(new QLabel(this)),
      save_(new QPushButton(QCoreApplication::translate("App", "Speichern"), this)),
      cancel_(new QPushButton(QCoreApplication::translate("App", "Abbrechen"), this)),
      photoView_(new PhotoView(this)),
      choosePhoto_(new QPushButton(QCoreApplication::translate("App", "&Foto hinzufügen"), this)),
      removePhoto_(new QPushButton(QCoreApplication::translate("App", "Foto &entfernen"), this)) {
  setObjectName(QStringLiteral("itemEditor"));
  setWindowTitle(existing ? QCoreApplication::translate("App", "Sammlerstück bearbeiten")
                          : QCoreApplication::translate("App", "Sammlerstück hinzufügen"));
  setWindowModality(Qt::WindowModal);
  resize(600, 780);
  setMinimumSize(320, 280);
  name_->setObjectName(QStringLiteral("itemName"));
  maker_->setObjectName(QStringLiteral("itemMaker"));
  location_->setObjectName(QStringLiteral("itemLocation"));
  note_->setObjectName(QStringLiteral("itemNote"));
  save_->setObjectName(QStringLiteral("saveItem"));
  cancel_->setObjectName(QStringLiteral("cancelEdit"));
  error_->setObjectName(QStringLiteral("editorError"));
  for (auto* field : {name_, maker_, location_}) {
    field->setMaxLength(1000000);
    field->setClearButtonEnabled(true);
  }
  name_->setPlaceholderText(QCoreApplication::translate("App", "Wie heißt das Sammlerstück?"));
  maker_->setPlaceholderText(QCoreApplication::translate("App", "Optional"));
  location_->setPlaceholderText(
      QCoreApplication::translate("App", "Zum Beispiel Wohnzimmer, Regal 2"));
  note_->setPlaceholderText(
      QCoreApplication::translate("App", "Was möchtest du dir zu diesem Sammlerstück merken?"));
  note_->setMinimumHeight(120);
  note_->setTabChangesFocus(true);
  error_->setTextFormat(Qt::PlainText);
  error_->setWordWrap(true);
  error_->setAccessibleName(QCoreApplication::translate("App", "Hinweis zum Speichern"));
  error_->hide();
  connect(name_, &QLineEdit::textChanged, this, [this] {
    if (validationError_ && !name_->text().trimmed().isEmpty()) {
      validationError_ = false;
      error_->hide();
    }
  });
  if (existing) {
    id_ = existing->id;
    photo_ = existing->fields.photo;
    name_->setText(existing->fields.name);
    maker_->setText(existing->fields.maker);
    location_->setText(existing->fields.location);
    note_->setPlainText(existing->fields.note);
  }
  original_ = draft();
  photoView_->setObjectName(QStringLiteral("draftPhoto"));
  photoView_->setMinimumHeight(160);
  choosePhoto_->setObjectName(QStringLiteral("choosePhoto"));
  removePhoto_->setObjectName(QStringLiteral("removePhoto"));
  removePhoto_->setEnabled(!photo_.isEmpty());
  if (!photo_.isEmpty()) {
    photoView_->showImage(preview);
    choosePhoto_->setText(QCoreApplication::translate("App", "&Foto ersetzen"));
  }
  choosePhoto_->setAutoDefault(false);
  removePhoto_->setAutoDefault(false);
  connect(choosePhoto_, &QPushButton::clicked, this, [this] {
    if (saving_ || importing_ || choosing_ || color_->isChoosing() || controller_.isBusy()) {
      return;
    }
    choosing_ = true;
    const auto path = QFileDialog::getOpenFileName(
        this, QCoreApplication::translate("App", "Foto auswählen"), {},
        QCoreApplication::translate("App", "Fotos (*.jpg *.jpeg *.png);;Alle Dateien (*)"));
    choosing_ = false;
    importPath(path);
  });
  connect(removePhoto_, &QPushButton::clicked, this, &ItemEditor::removePhoto);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(20, 20, 20, 20);
  root->setSpacing(14);
  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  auto* formWidget = new QWidget(scroll);
  auto* form = new QFormLayout(formWidget);
  form->setRowWrapPolicy(QFormLayout::WrapAllRows);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  form->setSpacing(10);
  auto* hint = new QLabel(
      QCoreApplication::translate("App", "Nur die Bezeichnung ist erforderlich."), formWidget);
  hint->setWordWrap(true);
  form->addRow(hint);
  form->addRow(QCoreApplication::translate("App", "&Bezeichnung *"), name_);
  form->addRow(QCoreApplication::translate("App", "&Marke oder Hersteller"), maker_);
  form->addRow(QCoreApplication::translate("App", "Farbe"), color_);
  form->addRow(QCoreApplication::translate("App", "&Aufbewahrungsort"), location_);
  form->addRow(QCoreApplication::translate("App", "&Notiz"), note_);
  form->addRow(photoView_);
  form->addRow(choosePhoto_);
  form->addRow(removePhoto_);
  auto* photoHint = new QLabel(
      QCoreApplication::translate(
          "App", "JPEG oder PNG · bis 10 MiB · 6000 Pixel je Seite · 16 Millionen Pixel insgesamt"),
      this);
  photoHint->setWordWrap(true);
  form->addRow(photoHint);
  form->addRow(error_);
  scroll->setWidget(formWidget);
  root->addWidget(scroll, 1);
  auto* buttons = new QDialogButtonBox(this);
  buttons->addButton(cancel_, QDialogButtonBox::RejectRole);
  buttons->addButton(save_, QDialogButtonBox::AcceptRole);
  root->addWidget(buttons);
  save_->setDefault(true);
  connect(save_, &QPushButton::clicked, this, &ItemEditor::save);
  connect(cancel_, &QPushButton::clicked, this, &ItemEditor::reject);
  auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
  connect(saveShortcut, &QShortcut::activated, this, &ItemEditor::save);
  connect(&controller_, &StorageController::saved, this, [this](const Collectible&) {
    if (saving_) {
      saving_ = false;
      accept();
    }
  });
  connect(&controller_, &StorageController::failed, this,
          [this, scroll](Operation operation, const QString& message) {
            if (operation == Operation::Save && saving_) {
              setSaving(false);
              error_->setText(message);
              error_->show();
              QTimer::singleShot(0, scroll,
                                 [this, scroll] { scroll->ensureWidgetVisible(error_); });
            }
          });
  connect(&controller_, &StorageController::imported, this,
          [this](quint64 token, const QByteArray& bytes, const QImage& image) {
            if (!importing_ || token != importToken_) {
              return;
            }
            photo_ = bytes;
            photoView_->showImage(image);
            importing_ = false;
            setSaving(false);
            error_->hide();
          });
  connect(&controller_, &StorageController::importFailed, this,
          [this, scroll](quint64 token, const QString& message) {
            if (!importing_ || token != importToken_) {
              return;
            }
            importing_ = false;
            setSaving(false);
            error_->setText(message);
            error_->show();
            QTimer::singleShot(0, scroll, [this, scroll] { scroll->ensureWidgetVisible(error_); });
          });
  color_->setTabOrderBetween(maker_, location_);
  name_->setFocus();
}

ItemDraft ItemEditor::draft() const {
  return {name_->text(),        maker_->text(), location_->text(),
          note_->toPlainText(), photo_,         color_->value()};
}

void ItemEditor::setSaving(bool saving) {
  saving_ = saving;
  const bool blocked = saving || importing_;
  choosePhoto_->setEnabled(!blocked);
  removePhoto_->setEnabled(!blocked && !photo_.isEmpty());
  choosePhoto_->setText(photo_.isEmpty() ? QCoreApplication::translate("App", "&Foto hinzufügen")
                                         : QCoreApplication::translate("App", "&Foto ersetzen"));
  color_->setEnabled(!blocked);
  name_->setEnabled(!blocked);
  maker_->setEnabled(!blocked);
  location_->setEnabled(!blocked);
  note_->setEnabled(!blocked);
  save_->setEnabled(!blocked);
  cancel_->setEnabled(!blocked);
  save_->setText(saving ? QCoreApplication::translate("App", "Wird gespeichert …")
                        : QCoreApplication::translate("App", "Speichern"));
}

void ItemEditor::save() {
  if (saving_ || importing_ || choosing_ || color_->isChoosing() || controller_.isBusy()) {
    return;
  }
  try {
    const auto validated = validateDraft(draft());
    validationError_ = false;
    error_->hide();
    setSaving(true);
    if (!controller_.save(id_, validated)) {
      setSaving(false);
    }
  } catch (const ValidationError& error) {
    validationError_ = true;
    error_->setText(QString::fromUtf8(error.what()));
    error_->show();
    name_->setFocus();
  }
}

bool ItemEditor::requestClose() {
  if (saving_ || importing_ || choosing_ || color_->isChoosing()) {
    return false;
  }
  if (draft() != original_ && !confirmDiscard(this)) {
    return false;
  }
  QDialog::reject();
  return true;
}

void ItemEditor::reject() { requestClose(); }

void ItemEditor::closeEvent(QCloseEvent* event) {
  if (requestClose()) {
    event->accept();
  } else {
    event->ignore();
  }
}

void ItemEditor::importPath(const QString& path) {
  if (path.isEmpty() || saving_ || importing_ || choosing_ || color_->isChoosing() ||
      controller_.isBusy()) {
    return;
  }
  static std::atomic<quint64> nextToken{0};
  importToken_ = ++nextToken;
  importing_ = true;
  validationError_ = false;
  setSaving(false);
  error_->setText(QCoreApplication::translate("App", "Foto wird geprüft …"));
  error_->show();
  if (!controller_.importFile(path, importToken_)) {
    importing_ = false;
    setSaving(false);
  }
}
void ItemEditor::removePhoto() {
  if (saving_ || importing_ || choosing_ || color_->isChoosing() || controller_.isBusy()) {
    return;
  }
  photo_.clear();
  photoView_->showMessage(QCoreApplication::translate("App", "Kein Foto"));
  setSaving(false);
}
} // namespace vitrine
