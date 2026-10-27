#include "ui/main_window.h"
#include "ui/ui_support.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFile>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace vitrine {

MainWindow::MainWindow(StorageController& controller, QString dataDirectory, QWidget* parent)
    : QMainWindow(parent), controller_(controller), dataDirectory_(std::move(dataDirectory)),
      search_(new QLineEdit(this)), table_(new QTableWidget(this)), note_(new QPlainTextEdit(this)),
      detailTitle_(new QLabel(this)), state_(new QLabel(this)), feedback_(new QLabel(this)),
      count_(new QLabel(this)),
      add_(new QPushButton(QStringLiteral("Sammlerstück hinzufügen"), this)),
      edit_(new QPushButton(QStringLiteral("Bearbeiten"), this)),
      remove_(new QPushButton(QStringLiteral("Löschen"), this)),
      reload_(new QPushButton(QStringLiteral("Neu laden"), this)), photoView_(new PhotoView(this)) {
  setWindowTitle(QStringLiteral("SammlerVitrine"));
  setObjectName(QStringLiteral("mainWindow"));
  resize(1100, 820);
  setMinimumSize(440, 420);
  auto* central = new QWidget(this);
  auto* root = new QVBoxLayout(central);
  root->setContentsMargins(24, 22, 24, 22);
  root->setSpacing(14);
  auto* header = new QHBoxLayout;
  auto* title = new QLabel(QStringLiteral("SammlerVitrine"), central);
  QFont titleFont = title->font();
  titleFont.setPointSize(22);
  titleFont.setBold(true);
  title->setFont(titleFont);
  header->addWidget(title, 1);
  auto* about = new QPushButton(QStringLiteral("Über die App"), central);
  header->addWidget(about);
  root->addLayout(header);
  auto* subtitle = new QLabel(
      QStringLiteral("Deine Sammlerstücke. Dein Platz für alles, was dazugehört."), central);
  subtitle->setWordWrap(true);
  root->addWidget(subtitle);
  search_->setObjectName(QStringLiteral("searchItems"));
  search_->setAccessibleName(QStringLiteral("Sammlung durchsuchen"));
  search_->setPlaceholderText(QStringLiteral("Name, Marke, Aufbewahrungsort oder Notiz suchen …"));
  search_->setClearButtonEnabled(true);
  root->addWidget(search_);
  auto* clear = new QPushButton(QStringLiteral("Suche leeren"), central);
  clear->setObjectName(QStringLiteral("clearSearch"));
  auto* searchInfo = new QHBoxLayout;
  searchInfo->addWidget(count_, 1);
  searchInfo->addWidget(clear);
  root->addLayout(searchInfo);

  auto* splitter = new QSplitter(Qt::Vertical, central);
  auto* collection = new QWidget(splitter);
  auto* collectionLayout = new QVBoxLayout(collection);
  collectionLayout->setContentsMargins(0, 0, 0, 0);
  state_->setObjectName(QStringLiteral("collectionState"));
  state_->setWordWrap(true);
  state_->setTextFormat(Qt::PlainText);
  collectionLayout->addWidget(state_);
  table_->setObjectName(QStringLiteral("itemsTable"));
  table_->setAccessibleName(QStringLiteral("Sammlung"));
  table_->setColumnCount(4);
  table_->setHorizontalHeaderLabels({QStringLiteral("Bezeichnung"),
                                     QStringLiteral("Marke / Hersteller"),
                                     QStringLiteral("Aufbewahrungsort"), QStringLiteral("Foto")});
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setAlternatingRowColors(true);
  table_->setShowGrid(false);
  table_->verticalHeader()->hide();
  table_->verticalHeader()->setDefaultSectionSize(table_->fontMetrics().height() + 20);
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table_->horizontalHeader()->setMinimumSectionSize(105);
  table_->installEventFilter(this);
  collectionLayout->addWidget(table_, 1);
  auto* detail = new QWidget(splitter);
  auto* detailLayout = new QVBoxLayout(detail);
  detailLayout->setContentsMargins(0, 0, 0, 0);
  detailTitle_->setTextFormat(Qt::PlainText);
  detailTitle_->setWordWrap(true);
  QFont detailFont = detailTitle_->font();
  detailFont.setBold(true);
  detailTitle_->setFont(detailFont);
  detailLayout->addWidget(detailTitle_);
  note_->setObjectName(QStringLiteral("selectedNote"));
  note_->setAccessibleName(QStringLiteral("Vollständige Notiz"));
  note_->setReadOnly(true);
  note_->setTabChangesFocus(true);
  auto* photoAndNote = new QSplitter(Qt::Horizontal, detail);
  photoView_->setObjectName(QStringLiteral("selectedPhoto"));
  photoAndNote->addWidget(photoView_);
  photoAndNote->addWidget(note_);
  photoAndNote->setStretchFactor(0, 1);
  photoAndNote->setStretchFactor(1, 2);
  detailLayout->addWidget(photoAndNote);
  splitter->addWidget(collection);
  splitter->addWidget(detail);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  splitter->setChildrenCollapsible(false);
  root->addWidget(splitter, 1);
  feedback_->setObjectName(QStringLiteral("mainFeedback"));
  feedback_->setTextFormat(Qt::PlainText);
  feedback_->setWordWrap(true);
  feedback_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  root->addWidget(feedback_);

  auto* actions = new QHBoxLayout;
  add_->setText(QStringLiteral("&Hinzufügen"));
  edit_->setText(QStringLiteral("&Bearbeiten"));
  remove_->setText(QStringLiteral("&Löschen"));
  add_->setObjectName(QStringLiteral("addItem"));
  edit_->setObjectName(QStringLiteral("editItem"));
  remove_->setObjectName(QStringLiteral("removeItem"));
  reload_->setObjectName(QStringLiteral("reloadItems"));
  actions->addWidget(add_);
  actions->addWidget(edit_);
  actions->addWidget(remove_);
  actions->addStretch();
  root->addLayout(actions);
  root->addWidget(reload_, 0, Qt::AlignRight);
  reload_->hide();
  auto* centralScroll = new QScrollArea(this);
  centralScroll->setFrameShape(QFrame::NoFrame);
  centralScroll->setWidgetResizable(true);
  centralScroll->setWidget(central);
  setCentralWidget(centralScroll);

  connect(about, &QPushButton::clicked, this, &MainWindow::showAbout);
  connect(clear, &QPushButton::clicked, search_, &QLineEdit::clear);
  connect(search_, &QLineEdit::textChanged, this, [this, clear] {
    clear->setEnabled(!search_->text().isEmpty());
    const auto selected = selectedItem();
    refresh(selected ? selected->id : 0);
  });
  clear->setEnabled(false);
  connect(table_, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateSelection);
  connect(table_, &QTableWidget::cellDoubleClicked, this, [this] { openEditor(true); });
  connect(add_, &QPushButton::clicked, this, [this] { openEditor(false); });
  connect(edit_, &QPushButton::clicked, this, [this] { openEditor(true); });
  connect(remove_, &QPushButton::clicked, this, &MainWindow::deleteSelected);
  connect(reload_, &QPushButton::clicked, &controller_, &StorageController::load);
  auto* newShortcut = new QShortcut(QKeySequence::New, this);
  connect(newShortcut, &QShortcut::activated, this, [this] { openEditor(false); });
  auto* findShortcut = new QShortcut(QKeySequence::Find, this);
  connect(findShortcut, &QShortcut::activated, this, [this] {
    search_->setFocus();
    search_->selectAll();
  });
  auto* quitShortcut = new QShortcut(QKeySequence::Quit, this);
  connect(quitShortcut, &QShortcut::activated, this, &QWidget::close);
  connect(&controller_, &StorageController::busyChanged, this, [this](bool busy) {
    updateActions();
    if (busy) {
      feedback_->setText(QStringLiteral("Daten werden verarbeitet …"));
    } else if (feedback_->text() == QStringLiteral("Daten werden verarbeitet …")) {
      feedback_->clear();
    }
  });
  connect(&controller_, &StorageController::loaded, this, [this](const ItemList& items) {
    const auto selected = selectedItem();
    items_ = items;
    ready_ = true;
    feedback_->clear();
    reload_->hide();
    refresh(selected ? selected->id : 0);
  });
  connect(&controller_, &StorageController::saved, this, [this](const Collectible& item) {
    auto found = std::find_if(items_.begin(), items_.end(),
                              [&item](const Collectible& row) { return row.id == item.id; });
    auto summary = item;
    summary.fields.photo.clear();
    if (found == items_.end()) {
      items_.append(summary);
    } else {
      *found = summary;
    }
    refresh(item.id);
    const bool visible = std::any_of(visibleItems_.cbegin(), visibleItems_.cend(),
                                     [&item](const Collectible& row) { return row.id == item.id; });
    feedback_->setText(
        visible ? QStringLiteral("„%1“ wurde gespeichert.").arg(item.fields.name)
                : QStringLiteral("„%1“ wurde gespeichert, passt aber nicht zur aktuellen Suche. "
                                 "Mit „Suche leeren“ findest du es wieder.")
                      .arg(item.fields.name));
    reload_->hide();
  });
  connect(&controller_, &StorageController::removed, this, [this](qint64 id) {
    const int row = table_->currentRow();
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [id](const Collectible& item) { return item.id == id; }),
                 items_.end());
    refresh(0, row);
    feedback_->setText(QStringLiteral("Das Sammlerstück wurde gelöscht."));
    reload_->hide();
  });
  connect(&controller_, &StorageController::failed, this,
          [this](Operation operation, const QString& message) {
            feedback_->setText(message);
            if (operation != Operation::Save) {
              reload_->show();
            }
            if (!ready_) {
              state_->setText(QStringLiteral("Die Sammlung konnte nicht geöffnet werden."));
              count_->clear();
            }
            updateActions();
          });
  connect(&controller_, &StorageController::itemRead, this,
          [this](quint64 token, const Collectible& item, const QImage& preview) {
            if (token != readToken_) {
              return;
            }
            if (item.hasPhoto) {
              photoView_->showImage(preview);
            } else {
              photoView_->showMessage(QStringLiteral("Kein Foto"));
            }
            if (editLoading_) {
              editLoading_ = false;
              showEditor(item, preview);
            }
          });
  connect(&controller_, &StorageController::readFailed, this,
          [this](quint64 token, const QString& message) {
            if (token != readToken_) {
              return;
            }
            photoView_->showMessage(
                QStringLiteral("Foto konnte nicht geladen werden.\n%1").arg(message));
            if (editLoading_) {
              feedback_->setText(QStringLiteral("Bearbeitung nicht geöffnet. %1").arg(message));
              editLoading_ = false;
              updateActions();
            }
          });
  refresh();
  QTimer::singleShot(0, &controller_, &StorageController::load);
}

std::optional<Collectible> MainWindow::selectedItem() const {
  const int row = table_->currentRow();
  if (row < 0 || row >= visibleItems_.size()) {
    return std::nullopt;
  }
  return visibleItems_.at(row);
}

void MainWindow::refresh(std::optional<qint64> preferredId, int fallbackRow) {
  const QSignalBlocker block(table_);
  visibleItems_ = searchItems(items_, search_->text());
  table_->setRowCount(static_cast<int>(visibleItems_.size()));
  int selection = visibleItems_.isEmpty()
                      ? -1
                      : std::clamp(fallbackRow, 0, static_cast<int>(visibleItems_.size()) - 1);
  for (int row = 0; row < visibleItems_.size(); ++row) {
    const auto& item = visibleItems_.at(row);
    const QStringList values{item.fields.name, item.fields.maker, item.fields.location,
                             item.hasPhoto ? QStringLiteral("Ja") : QStringLiteral("—")};
    for (int column = 0; column < values.size(); ++column) {
      auto* cell = new QTableWidgetItem(values.at(column));
      cell->setData(Qt::UserRole, item.id);
      cell->setToolTip(values.at(column));
      table_->setItem(row, column, cell);
    }
    if (preferredId && item.id == *preferredId) {
      selection = row;
    }
  }
  table_->clearSelection();
  if (selection >= 0) {
    table_->setCurrentCell(selection, 0);
    table_->selectRow(selection);
  } else {
    table_->setCurrentCell(-1, -1);
  }
  state_->setText(
      !ready_ ? QStringLiteral("Sammlung wird geladen …")
      : items_.isEmpty()
          ? QStringLiteral("Deine Vitrine ist noch leer. Füge dein erstes Sammlerstück hinzu.")
      : visibleItems_.isEmpty()
          ? QStringLiteral("Keine Sammlerstücke gefunden. Ändere oder leere die Suche.")
          : QString());
  count_->setText(
      ready_
          ? QStringLiteral("%1 von %2 Sammlerstücken").arg(visibleItems_.size()).arg(items_.size())
          : QString());
  updateSelection();
}

void MainWindow::updateSelection() {
  const auto selected = selectedItem();
  ++readToken_;
  editLoading_ = false;
  if (selected && selected->hasPhoto) {
    photoView_->showMessage(QStringLiteral("Foto wird geladen …"));
    controller_.readItem({selected->id, readToken_});
  } else {
    photoView_->showMessage(selected ? QStringLiteral("Kein Foto")
                                     : QStringLiteral("Wähle ein Sammlerstück aus."));
  }
  detailTitle_->setText(selected
                            ? QStringLiteral("Foto und Notiz zu „%1“").arg(selected->fields.name)
                            : QStringLiteral("Notiz"));
  note_->setPlainText(selected ? selected->fields.note : QString());
  note_->setPlaceholderText(
      selected ? QStringLiteral("Zu diesem Sammlerstück gibt es noch keine Notiz.")
               : QStringLiteral("Wähle ein Sammlerstück aus, um seine Notiz zu lesen."));
  updateActions();
}

void MainWindow::updateActions() {
  const bool available =
      ready_ && !editLoading_ && !controller_.isBusy() && (!editor_ || !editor_->isVisible());
  add_->setEnabled(available);
  edit_->setEnabled(available && selectedItem().has_value());
  remove_->setEnabled(available && selectedItem().has_value());
  reload_->setEnabled(!controller_.isBusy() && (!editor_ || !editor_->isVisible()));
}

void MainWindow::openEditor(bool editing) {
  if (!ready_ || editLoading_ || controller_.isBusy() || (editor_ && editor_->isVisible())) {
    return;
  }
  const auto selected = editing ? selectedItem() : std::nullopt;
  if (editing && !selected) {
    return;
  }
  if (selected && selected->hasPhoto) {
    editLoading_ = true;
    updateActions();
    controller_.readItem({selected->id, ++readToken_});
    return;
  }
  showEditor(selected);
}

void MainWindow::showEditor(std::optional<Collectible> item, const QImage& preview) {
  editor_ = new ItemEditor(controller_, std::move(item), this, preview);
  editor_->setAttribute(Qt::WA_DeleteOnClose);
  connect(editor_, &QDialog::finished, this, [this] {
    editor_ = nullptr;
    updateActions();
  });
  editor_->open();
  updateActions();
}

void MainWindow::deleteSelected() {
  const auto selected = selectedItem();
  if (!selected || !remove_->isEnabled()) {
    return;
  }
  if (confirmDelete(this, selected->fields.name)) {
    controller_.remove(selected->id);
  }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (watched == table_ && event->type() == QEvent::KeyPress) {
    const auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
      openEditor(true);
      return true;
    }
    if (key->key() == Qt::Key_Delete ||
        (key->key() == Qt::Key_Backspace && key->modifiers().testFlag(Qt::ControlModifier))) {
      deleteSelected();
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (controller_.isBusy()) {
    feedback_->setText(QStringLiteral(
        "Bitte warte, bis der laufende Datenbankzugriff beendet ist, und schließe dann erneut."));
    event->ignore();
    return;
  }
  if (editor_ && editor_->isVisible() && !editor_->requestClose()) {
    event->ignore();
    return;
  }
  event->accept();
}

void MainWindow::showAbout() {
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("Über SammlerVitrine"));
  dialog.resize(640, 500);
  auto* layout = new QVBoxLayout(&dialog);
  auto* text = new QTextBrowser(&dialog);
  QFile notices(QStringLiteral(":/licenses/THIRD_PARTY_NOTICES.md"));
  QString licenseText;
  if (notices.open(QIODevice::ReadOnly)) {
    licenseText = QString::fromUtf8(notices.readAll());
  }
  text->setPlainText(
      QStringLiteral(
          "SammlerVitrine %1\n\nDeine Sammlung bleibt lokal.\nDatenverzeichnis: %2\n\n%3")
          .arg(QCoreApplication::applicationVersion(), dataDirectory_, licenseText));
  layout->addWidget(text);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("Schließen"));
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  dialog.exec();
}

} // namespace vitrine
