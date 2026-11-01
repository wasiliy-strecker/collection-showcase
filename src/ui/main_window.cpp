#include "ui/main_window.h"
#include "ui/color_field.h"
#include "ui/export_file_dialog.h"
#include "ui/information_dialog.h"
#include "ui/language_dialog.h"
#include "ui/sort_header.h"
#include "ui/ui_support.h"
#include <QAction>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLocale>
#include <QMenu>
#include <QStandardPaths>

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

MainWindow::MainWindow(StorageController& controller, QString dataDirectory,
                       QString languagePreference, QWidget* parent)
    : QMainWindow(parent), controller_(controller), dataDirectory_(std::move(dataDirectory)),
      languagePreference_(std::move(languagePreference)), search_(new QLineEdit(this)),
      table_(new QTableWidget(this)), note_(new QPlainTextEdit(this)),
      detailTitle_(new QLabel(this)), state_(new QLabel(this)), feedback_(new QLabel(this)),
      count_(new QLabel(this)),
      add_(new QPushButton(QCoreApplication::translate("App", "Sammlerstück hinzufügen"), this)),
      edit_(new QPushButton(QCoreApplication::translate("App", "Bearbeiten"), this)),
      remove_(new QPushButton(QCoreApplication::translate("App", "Löschen"), this)),
      reload_(new QPushButton(QCoreApplication::translate("App", "Neu laden"), this)),
      language_(new QPushButton(QCoreApplication::translate("App", "Sprache"), this)),
      photoView_(new PhotoView(this)) {
  exportCollection_ = new QPushButton(QCoreApplication::translate("App", "Exportieren"), this);
  importCollection_ = new QPushButton(QCoreApplication::translate("App", "Importieren"), this);
  transferDirectory_ = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  setWindowTitle(QStringLiteral("Collection Showcase"));
  setObjectName(QStringLiteral("mainWindow"));
  resize(1100, 820);
  setMinimumSize(440, 420);
  auto* central = new QWidget(this);
  auto* root = new QVBoxLayout(central);
  root->setContentsMargins(24, 22, 24, 22);
  root->setSpacing(14);
  auto* header = new QHBoxLayout;
  auto* title = new QLabel(QStringLiteral("Collection Showcase"), central);
  QFont titleFont = title->font();
  titleFont.setPointSize(22);
  titleFont.setBold(true);
  title->setFont(titleFont);
  header->addWidget(title, 1);
  auto* about = new QPushButton(QCoreApplication::translate("App", "Info"), central);
  about->setAccessibleName(QCoreApplication::translate("App", "Info"));
  auto* infoMenu = new QMenu(about);
  about->setMenu(infoMenu);
  auto* aboutAction = infoMenu->addAction(QCoreApplication::translate("App", "Über die App"));
  aboutAction->setObjectName(QStringLiteral("showAbout"));
  auto* privacyAction = infoMenu->addAction(QCoreApplication::translate("App", "Datenschutz"));
  privacyAction->setObjectName(QStringLiteral("showPrivacy"));
  auto* licensesAction = infoMenu->addAction(QCoreApplication::translate("App", "Lizenzen"));
  licensesAction->setObjectName(QStringLiteral("showLicenses"));
  about->setObjectName(QStringLiteral("aboutApp"));
  root->addLayout(header);
  language_->setObjectName(QStringLiteral("changeLanguage"));
  auto* subtitle =
      new QLabel(QCoreApplication::translate(
                     "App", "Deine Sammlerstücke. Dein Platz für alles, was dazugehört."),
                 central);
  subtitle->setWordWrap(true);
  root->addWidget(subtitle);
  search_->setObjectName(QStringLiteral("searchItems"));
  search_->setAccessibleName(QCoreApplication::translate("App", "Sammlung durchsuchen"));
  search_->setPlaceholderText(
      QCoreApplication::translate("App", "Name, Marke, Aufbewahrungsort oder Notiz suchen …"));
  search_->setClearButtonEnabled(true);

  auto* clear = new QPushButton(QCoreApplication::translate("App", "Suche leeren"), central);
  clear->setObjectName(QStringLiteral("clearSearch"));
  auto* searchRow = new QHBoxLayout;
  searchRow->addWidget(search_, 1);
  searchRow->addWidget(clear);
  search_->setMinimumHeight(36);
  root->addLayout(searchRow);

  auto* splitter = new QSplitter(Qt::Vertical, central);
  auto* collection = new QWidget(splitter);
  auto* collectionLayout = new QVBoxLayout(collection);
  collectionLayout->setContentsMargins(0, 0, 0, 0);
  state_->setObjectName(QStringLiteral("collectionState"));
  state_->setWordWrap(true);
  state_->setTextFormat(Qt::PlainText);
  collectionLayout->addWidget(state_);
  table_->setObjectName(QStringLiteral("itemsTable"));
  table_->setAccessibleName(QCoreApplication::translate("App", "Sammlung"));
  table_->setColumnCount(5);
  auto* sortHeader = new SortHeader(table_);
  table_->setHorizontalHeader(sortHeader);
  table_->setHorizontalHeaderLabels({QCoreApplication::translate("App", "Bezeichnung"),
                                     QCoreApplication::translate("App", "Marke / Hersteller"),
                                     QCoreApplication::translate("App", "Farbe"),
                                     QCoreApplication::translate("App", "Aufbewahrungsort"),
                                     QCoreApplication::translate("App", "Foto")});
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setAlternatingRowColors(true);
  table_->setShowGrid(false);
  table_->verticalHeader()->hide();
  table_->verticalHeader()->setDefaultSectionSize(table_->fontMetrics().height() + 20);
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table_->horizontalHeader()->setMinimumSectionSize(105);
  table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  table_->setIconSize(QSize(24, 24));
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
  note_->setAccessibleName(QCoreApplication::translate("App", "Vollständige Notiz"));
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
  add_->setText(QCoreApplication::translate("App", "&Hinzufügen"));
  edit_->setText(QCoreApplication::translate("App", "&Bearbeiten"));
  remove_->setText(QCoreApplication::translate("App", "&Löschen"));
  add_->setObjectName(QStringLiteral("addItem"));
  edit_->setObjectName(QStringLiteral("editItem"));
  remove_->setObjectName(QStringLiteral("removeItem"));
  reload_->setObjectName(QStringLiteral("reloadItems"));
  header->addWidget(add_);
  actions->addWidget(count_, 1);
  actions->addWidget(edit_);
  actions->addWidget(remove_);

  collectionLayout->addLayout(actions);
  auto* footer = new QHBoxLayout;
  auto* localStorage = new QLabel(
      QCoreApplication::translate("App", "Lokal auf diesem Computer gespeichert"), central);
  localStorage->setWordWrap(true);
  footer->addWidget(localStorage, 1);
  footer->addWidget(about);
  footer->addWidget(language_);
  exportCollection_->setObjectName(QStringLiteral("exportCollection"));
  importCollection_->setObjectName(QStringLiteral("importCollection"));
  footer->addWidget(exportCollection_);
  footer->addWidget(importCollection_);
  root->addLayout(footer);
  for (auto* button :
       {add_, edit_, remove_, clear, about, language_, exportCollection_, importCollection_})
    button->setMinimumHeight(34);
  root->addWidget(reload_, 0, Qt::AlignRight);
  reload_->hide();
  auto* centralScroll = new QScrollArea(this);
  centralScroll->setFrameShape(QFrame::NoFrame);
  centralScroll->setWidgetResizable(true);
  centralScroll->setWidget(central);
  setCentralWidget(centralScroll);

  connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
  connect(privacyAction, &QAction::triggered, this, [this] {
    InformationDialog dialog(InformationPage::Privacy, dataDirectory_, this);
    dialog.exec();
  });
  connect(licensesAction, &QAction::triggered, this, [this] {
    InformationDialog dialog(InformationPage::Licenses, dataDirectory_, this);
    dialog.exec();
  });
  connect(language_, &QPushButton::clicked, this, &MainWindow::showLanguage);
  connect(exportCollection_, &QPushButton::clicked, this, &MainWindow::exportCollection);
  connect(importCollection_, &QPushButton::clicked, this, &MainWindow::importCollection);
  connect(&controller_, &StorageController::collectionExportInspected, this,
          [this](const CollectionExportTarget& target) {
            if (!transferRunning_) {
              return;
            }
            if (target.exists &&
                !confirmExportReplacement(this, QFileInfo(target.path).fileName())) {
              finishTransfer(QCoreApplication::translate("App", "Vorgang abgebrochen."));
              return;
            }
            controller_.exportCollection(target, target.exists, dataDirectory_);
          });
  connect(&controller_, &StorageController::collectionExported, this, [this](qsizetype count) {
    finishTransfer(QCoreApplication::translate("App", "Export abgeschlossen. Einträge: %1.")
                       .arg(QLocale().toString(count)));
  });
  connect(&controller_, &StorageController::collectionImportReady, this, [this](qsizetype count) {
    if (!transferRunning_) {
      controller_.discardCollectionImport();
      return;
    }
    if (!confirmImport(this, count)) {
      controller_.discardCollectionImport();
      finishTransfer(QCoreApplication::translate("App", "Vorgang abgebrochen."));
      return;
    }
    controller_.importCollection();
  });
  connect(&controller_, &StorageController::collectionImported, this,
          [this](const CollectionImportResult& result) {
            const auto selected = selectedItem();
            items_ = result.entries;
            if (search_->text().isEmpty()) {
              refresh(selected ? selected->id : 0);
            } else {
              // Update the table and the line edit's own clear icon through the search handler.
              search_->clear();
            }
            finishTransfer(
                QCoreApplication::translate(
                    "App", "Import abgeschlossen. Hinzugefügt: %1. Bereits vorhanden: %2.")
                    .arg(QLocale().toString(result.added), QLocale().toString(result.skipped)));
          });
  auto* languageShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+,")), this);
  connect(languageShortcut, &QShortcut::activated, this, &MainWindow::showLanguage);
  connect(&controller_, &StorageController::languageSaved, this, [this](const QString& preference) {
    languagePreference_ = preference;
    feedback_->setText(
        QCoreApplication::translate("App", "Sprache gespeichert. Sie gilt beim nächsten Start."));
  });
  connect(clear, &QPushButton::clicked, search_, &QLineEdit::clear);
  connect(search_, &QLineEdit::textChanged, this, [this, clear] {
    clear->setEnabled(!search_->text().isEmpty());
    const auto selected = selectedItem();
    refresh(selected ? selected->id : 0);
  });
  clear->setEnabled(false);
  connect(sortHeader, &QHeaderView::sortIndicatorChanged, this, [this] {
    const auto selected = selectedItem();
    refresh(selected ? selected->id : 0);
  });
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
      feedback_->setText(QCoreApplication::translate("App", "Daten werden verarbeitet …"));
    } else if (feedback_->text() ==
               QCoreApplication::translate("App", "Daten werden verarbeitet …")) {
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
        visible
            ? QCoreApplication::translate("App", "„%1“ wurde gespeichert.").arg(item.fields.name)
            : QCoreApplication::translate(
                  "App", "„%1“ wurde gespeichert, passt aber nicht zur aktuellen Suche. "
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
    feedback_->setText(QCoreApplication::translate("App", "Das Sammlerstück wurde gelöscht."));
    reload_->hide();
  });
  connect(&controller_, &StorageController::failed, this,
          [this](Operation operation, const QString& message) {
            if (operation == Operation::InspectCollectionExport ||
                operation == Operation::ExportCollection ||
                operation == Operation::ReadCollectionImport ||
                operation == Operation::ImportCollection) {
              finishTransfer(message);
              return;
            }
            feedback_->setText(message);
            if (operation != Operation::Save && operation != Operation::Language) {
              reload_->show();
            }
            if (!ready_) {
              state_->setText(
                  QCoreApplication::translate("App", "Die Sammlung konnte nicht geöffnet werden."));
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
              photoView_->showMessage(QCoreApplication::translate("App", "Kein Foto"));
            }
            if (editLoading_) {
              editLoading_ = false;
              showEditor(item, preview);
            }
          });
  connect(
      &controller_, &StorageController::readFailed, this,
      [this](quint64 token, const QString& message) {
        if (token != readToken_) {
          return;
        }
        photoView_->showMessage(
            QCoreApplication::translate("App", "Foto konnte nicht geladen werden.\n%1")
                .arg(message));
        if (editLoading_) {
          feedback_->setText(
              QCoreApplication::translate("App", "Bearbeitung nicht geöffnet. %1").arg(message));
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
  visibleItems_ =
      searchItems(items_, search_->text(),
                  static_cast<SortField>(table_->horizontalHeader()->sortIndicatorSection()),
                  table_->horizontalHeader()->sortIndicatorOrder());
  table_->setRowCount(static_cast<int>(visibleItems_.size()));
  int selection = visibleItems_.isEmpty()
                      ? -1
                      : std::clamp(fallbackRow, 0, static_cast<int>(visibleItems_.size()) - 1);
  for (int row = 0; row < visibleItems_.size(); ++row) {
    const auto& item = visibleItems_.at(row);
    const QStringList values{
        item.fields.name, item.fields.maker, item.fields.color, item.fields.location,
        item.hasPhoto ? QCoreApplication::translate("App", "Ja") : QStringLiteral("—")};
    for (int column = 0; column < values.size(); ++column) {
      auto* cell = new QTableWidgetItem(values.at(column));
      cell->setData(Qt::UserRole, item.id);
      cell->setToolTip(values.at(column));
      if (column == static_cast<int>(SortField::Color))
        cell->setIcon(colorIcon(item.fields.color));
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
      !ready_ ? QCoreApplication::translate("App", "Sammlung wird geladen …")
      : items_.isEmpty()
          ? QCoreApplication::translate(
                "App", "Deine Vitrine ist noch leer. Füge dein erstes Sammlerstück hinzu.")
      : visibleItems_.isEmpty()
          ? QCoreApplication::translate(
                "App", "Keine Sammlerstücke gefunden. Ändere oder leere die Suche.")
          : QString());
  count_->setText(
      ready_ ? QCoreApplication::translate("App", "Sammlerstücke: %1 / %2")
                   .arg(QLocale().toString(visibleItems_.size()), QLocale().toString(items_.size()))
             : QString());
  updateSelection();
}

void MainWindow::updateSelection() {
  const auto selected = selectedItem();
  ++readToken_;
  editLoading_ = false;
  if (selected && selected->hasPhoto) {
    photoView_->showMessage(QCoreApplication::translate("App", "Foto wird geladen …"));
    controller_.readItem({selected->id, readToken_});
  } else {
    photoView_->showMessage(
        selected ? QCoreApplication::translate("App", "Kein Foto")
                 : QCoreApplication::translate("App", "Wähle ein Sammlerstück aus."));
  }
  detailTitle_->setText(
      selected
          ? QCoreApplication::translate("App", "Foto und Notiz zu „%1“").arg(selected->fields.name)
          : QCoreApplication::translate("App", "Notiz"));
  note_->setPlainText(selected ? selected->fields.note : QString());
  note_->setPlaceholderText(
      selected
          ? QCoreApplication::translate("App", "Zu diesem Sammlerstück gibt es noch keine Notiz.")
          : QCoreApplication::translate("App",
                                        "Wähle ein Sammlerstück aus, um seine Notiz zu lesen."));
  updateActions();
}

void MainWindow::updateActions() {
  const bool available = ready_ && !transferRunning_ && !editLoading_ && !controller_.isBusy() &&
                         (!editor_ || !editor_->isVisible());
  add_->setEnabled(available);
  language_->setEnabled(available);
  exportCollection_->setEnabled(available);
  importCollection_->setEnabled(available);
  edit_->setEnabled(available && selectedItem().has_value());
  remove_->setEnabled(available && selectedItem().has_value());
  reload_->setEnabled(!transferRunning_ && !controller_.isBusy() &&
                      (!editor_ || !editor_->isVisible()));
}

void MainWindow::openEditor(bool editing) {
  if (!ready_ || transferRunning_ || editLoading_ || controller_.isBusy() ||
      (editor_ && editor_->isVisible())) {
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
  if (transferRunning_ || controller_.isBusy()) {
    feedback_->setText(QCoreApplication::translate(
        "App",
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
  InformationDialog dialog(InformationPage::About, dataDirectory_, this);
  dialog.exec();
}

void MainWindow::showLanguage() {
  if (!language_->isEnabled()) {
    return;
  }
  LanguageDialog dialog(controller_, dataDirectory_, languagePreference_, this);
  dialog.exec();
}

bool MainWindow::beginTransfer() {
  if (!ready_ || editLoading_ || transferRunning_ || controller_.isBusy() ||
      (editor_ && editor_->isVisible())) {
    return false;
  }
  transferRunning_ = true;
  updateActions();
  return true;
}

void MainWindow::finishTransfer(const QString& message) {
  transferRunning_ = false;
  feedback_->setText(message);
  updateActions();
}

void MainWindow::exportCollection() {
  if (!beginTransfer()) {
    return;
  }
#ifdef Q_OS_MACOS
  constexpr bool selectDirectory = true;
#else
  constexpr bool selectDirectory = false;
#endif
  const auto path = chooseExportFile(
      this, QDir(transferDirectory_),
      QStringLiteral("collection-showcase-%1.json")
          .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))),
      selectDirectory);
  // Finish native-dialog cleanup before starting work that can open a confirmation.
  if (path.isEmpty()) {
    finishTransfer(QCoreApplication::translate("App", "Vorgang abgebrochen."));
    return;
  }
  transferDirectory_ = QFileInfo(path).absolutePath();
  controller_.inspectCollectionExport(path, dataDirectory_);
}

void MainWindow::importCollection() {
  if (!beginTransfer()) {
    return;
  }
  QString path;
  {
    QFileDialog dialog(
        this, QCoreApplication::translate("App", "Collection Showcase-Sammlung importieren"));
    dialog.setObjectName(QStringLiteral("importCollectionDialog"));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter(
        QCoreApplication::translate("App", "Collection Showcase-Sammlung (*.json *.JSON)"));
    dialog.setDirectory(transferDirectory_);
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
      path = dialog.selectedFiles().first();
    }
  }
  // Finish native-dialog cleanup before starting work that can open a confirmation.
  if (path.isEmpty()) {
    finishTransfer(QCoreApplication::translate("App", "Vorgang abgebrochen."));
    return;
  }
  transferDirectory_ = QFileInfo(path).absolutePath();
  controller_.readCollectionImport(path);
}

} // namespace vitrine
