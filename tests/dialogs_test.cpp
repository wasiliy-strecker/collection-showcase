#include "persistence/sqlite_item_repository.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QBuffer>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <atomic>
#include <memory>

using namespace vitrine;

namespace {
struct Controls {
  std::atomic<bool> failWrites{false};
  std::atomic<int> delay{0};
  std::atomic<int> readDelay{0};
  std::atomic<bool> failReads{false};
  std::atomic<int> writes{0};
  std::atomic<bool> wrongThread{false};
  std::atomic<bool> destroyed{false};
};

class ControlledRepository final : public ItemRepository {
public:
  TransferDraftList transferSnapshot() override {
    throw std::logic_error("Unexpected test transfer");
  }
  CollectionImportResult importEntries(const TransferDraftList&) override {
    throw std::logic_error("Unexpected test transfer");
  }
  ControlledRepository(const QString& directory, std::shared_ptr<Controls> controls)
      : repository_(directory), controls_(std::move(controls)), thread_(QThread::currentThread()) {
    if (thread_ == qApp->thread()) {
      controls_->wrongThread = true;
    }
  }
  ~ControlledRepository() override {
    checkThread();
    controls_->destroyed = true;
  }
  void initialize() override {
    checkThread();
    repository_.initialize();
  }
  ItemList findAll() override {
    checkThread();
    return repository_.findAll();
  }
  Collectible find(qint64 id) override {
    checkThread();
    QThread::msleep(static_cast<unsigned long>(controls_->readDelay.load()));
    if (controls_->failReads) {
      throw StorageError(QStringLiteral("Synthetischer Fotolesefehler"));
    }
    return repository_.find(id);
  }
  Collectible create(const ItemDraft& draft) override {
    beforeWrite();
    auto result = repository_.create(draft);
    ++controls_->writes;
    return result;
  }
  Collectible update(qint64 id, const ItemDraft& draft) override {
    beforeWrite();
    auto result = repository_.update(id, draft);
    ++controls_->writes;
    return result;
  }
  void remove(qint64 id) override {
    beforeWrite();
    repository_.remove(id);
    ++controls_->writes;
  }

private:
  void checkThread() {
    if (QThread::currentThread() != thread_) {
      controls_->wrongThread = true;
    }
  }
  void beforeWrite() {
    checkThread();
    QThread::msleep(static_cast<unsigned long>(controls_->delay.load()));
    if (controls_->failWrites) {
      throw StorageError(QStringLiteral("Synthetischer Speicherfehler. Bitte erneut versuchen."));
    }
  }
  SqliteItemRepository repository_;
  std::shared_ptr<Controls> controls_;
  QThread* thread_;
};

RepositoryFactory factory(const QString& directory, const std::shared_ptr<Controls>& controls) {
  return
      [directory, controls] { return std::make_unique<ControlledRepository>(directory, controls); };
}

template <typename Widget> Widget* child(QWidget& parent, const char* name) {
  auto* result = parent.findChild<Widget*>(QString::fromLatin1(name));
  Q_ASSERT(result);
  return result;
}

QString makePhoto(const QTemporaryDir& directory, const QColor& color,
                  const QString& name = QStringLiteral("source.png")) {
  QImage image(40, 20, QImage::Format_RGB32);
  image.fill(color);
  const auto path = directory.filePath(name);
  if (!image.save(path, "PNG")) {
    return {};
  }
  return path;
}
QByteArray bytesOf(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}

void fill(ItemEditor& editor, const QString& name = QStringLiteral("Azul")) {
  child<QLineEdit>(editor, "itemName")->setText(name);
  child<QLineEdit>(editor, "itemMaker")->setText(QStringLiteral("Müller"));
  child<QLineEdit>(editor, "itemLocation")->setText(QStringLiteral("Regal 2"));
  child<QPlainTextEdit>(editor, "itemNote")
      ->setPlainText(QStringLiteral("  Erste Zeile\n\nGeschenk von Ömer.\n"));
}

void answerConfirmation(const QString& objectName, const QString& buttonName,
                        const QString& expectedDefault, bool* checked) {
  QTimer::singleShot(0, qApp, [objectName, buttonName, expectedDefault, checked] {
    auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!box || box->objectName() != objectName) {
      return;
    }
    auto* button = box->findChild<QPushButton*>(buttonName);
    *checked = box->defaultButton() && box->defaultButton()->objectName() == expectedDefault &&
               box->escapeButton() == box->defaultButton();
    if (button) {
      button->click();
    }
  });
}
} // namespace

class DialogsTest final : public QObject {
  Q_OBJECT
private slots:
  void photoImportCancelFailureAndRetry() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy imported(&controller, &StorageController::imported);
    QSignalSpy importFailed(&controller, &StorageController::importFailed);
    QSignalSpy failed(&controller, &StorageController::failed);
    QSignalSpy saved(&controller, &StorageController::saved);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    ItemEditor editor(controller, std::nullopt);
    editor.show();
    fill(editor);
    const auto red = makePhoto(directory, Qt::red);
    editor.importPath(red);
    editor.save(); // blocked until complete import
    QVERIFY(QTest::qWaitFor([&] { return (imported.size()) == (1); }, 5000));
    QCOMPARE(saved.size(), 0);
    const auto redDraft = editor.draft();
    QVERIFY(!redDraft.photo.isEmpty());
    editor.importPath({}); // native file chooser cancellation forwards no path
    QCOMPARE(editor.draft(), redDraft);
    editor.importPath(directory.filePath("missing.png"));
    QVERIFY(QTest::qWaitFor([&] { return (importFailed.size()) == (1); }, 5000));
    QCOMPARE(editor.draft(), redDraft);
    const auto blue = makePhoto(directory, Qt::blue); // same file name, different bytes
    editor.importPath(blue);
    QVERIFY(QTest::qWaitFor([&] { return (imported.size()) == (2); }, 5000));
    QVERIFY(editor.draft().photo != redDraft.photo);
    const auto blueDraft = editor.draft();
    controls->failWrites = true;
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return (failed.size()) == (1); }, 5000));
    QCOMPARE(editor.draft(), blueDraft);
    QVERIFY(!child<PhotoView>(editor, "draftPhoto")->image().isNull());
    controls->failWrites = false;
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return (saved.size()) == (1); }, 5000));
    QCOMPARE(qvariant_cast<Collectible>(saved.first().first()).fields, blueDraft);
    QVERIFY(!editor.isVisible());
    QVERIFY(!controls->wrongThread);
  }
  void photoOnlyChangesAreProtected_data() {
    QTest::addColumn<int>("change");
    QTest::newRow("add") << 0;
    QTest::newRow("replace-same-filename") << 1;
    QTest::newRow("remove") << 2;
  }
  void photoOnlyChangesAreProtected() {
    QFETCH(int, change);
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    auto path = makePhoto(directory, Qt::red);
    Collectible original{
        8,
        {QStringLiteral("Figur"), {}, {}, {}, change == 0 ? QByteArray() : bytesOf(path)},
        change != 0};
    ItemEditor editor(controller, original, nullptr, QImage(path));
    editor.show();
    if (change == 2) {
      editor.removePhoto();
    } else {
      path = makePhoto(directory, Qt::blue);
      QSignalSpy imported(&controller, &StorageController::imported);
      editor.importPath(path);
      QVERIFY(QTest::qWaitFor([&] { return (imported.size()) == (1); }, 5000));
    }
    QVERIFY(editor.draft() != original.fields);
    bool checked = false;
    answerConfirmation(QStringLiteral("discardConfirmation"), QStringLiteral("keepEditing"),
                       QStringLiteral("keepEditing"), &checked);
    QVERIFY(!editor.requestClose());
    QVERIFY(checked);
    answerConfirmation(QStringLiteral("discardConfirmation"), QStringLiteral("discardChanges"),
                       QStringLiteral("keepEditing"), &checked);
    QVERIFY(editor.requestClose());
    QCOMPARE(controls->writes.load(), 0);
  }
  void photoRestoredExactlyClosesWithoutQuestion() {
    QTemporaryDir directory;
    const auto path = makePhoto(directory, Qt::red);
    const Collectible original{8, {QStringLiteral("Figur"), {}, {}, {}, bytesOf(path)}, true};
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy imported(&controller, &StorageController::imported);
    ItemEditor editor(controller, original, nullptr, QImage(path));
    editor.show();
    editor.removePhoto();
    editor.importPath(path);
    QVERIFY(QTest::qWaitFor([&] { return (imported.size()) == (1); }, 5000));
    QCOMPARE(editor.draft(), original.fields);
    QVERIFY(editor.requestClose());
  }
  void selectionRejectsStalePreviewAndReadErrorBlocksEditor() {
    QTemporaryDir directory;
    const auto red = makePhoto(directory, Qt::red, QStringLiteral("red.png"));
    const auto blue = makePhoto(directory, Qt::blue, QStringLiteral("blue.png"));
    {
      SqliteItemRepository repository(directory.path());
      repository.initialize();
      repository.create({QStringLiteral("A"), {}, {}, {}, bytesOf(red)});
      repository.create({QStringLiteral("B"), {}, {}, {}, bytesOf(blue)});
    }
    auto controls = std::make_shared<Controls>();
    controls->readDelay = 100;
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy read(&controller, &StorageController::itemRead);
    MainWindow window(controller, directory.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    auto* table = child<QTableWidget>(window, "itemsTable");
    auto* photo = child<PhotoView>(window, "selectedPhoto");
    table->selectRow(1);
    QVERIFY(QTest::qWaitFor([&] { return read.size() >= 2; }, 5000));
    QVERIFY(QTest::qWaitFor([&] { return !photo->image().isNull(); }, 5000));
    QCOMPARE(photo->image().pixelColor(0, 0), QColor(Qt::blue));
    table->selectRow(0);
    child<QLineEdit>(window, "searchItems")->setText(QStringLiteral("keine Treffer"));
    QTest::qWait(200);
    QVERIFY(photo->image().isNull());
    child<QLineEdit>(window, "searchItems")->clear();
    controls->failReads = true;
    child<QPushButton>(window, "editItem")->click();
    QVERIFY(QTest::qWaitFor(
        [&] {
          return child<QLabel>(window, "mainFeedback")
              ->text()
              .contains(QStringLiteral("Bearbeitung nicht geöffnet"));
        },
        5000));
    QVERIFY(!window.findChild<ItemEditor*>());
    QVERIFY(photo->message().contains(QStringLiteral("nicht geladen")));
    QVERIFY(!controls->wrongThread);
  }
  void destroyedPhotoReceiversIgnoreLateResults() {
    QTemporaryDir directory;
    const auto path = makePhoto(directory, Qt::green);
    auto controls = std::make_shared<Controls>();
    {
      SqliteItemRepository repository(directory.path());
      repository.initialize();
      repository.create({QStringLiteral("Figur"), {}, {}, {}, bytesOf(path)});
    }
    controls->readDelay = 150;
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy read(&controller, &StorageController::itemRead);
    auto window = std::make_unique<MainWindow>(controller, directory.path());
    window->show();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    window.reset();
    QVERIFY(QTest::qWaitFor([&] { return (read.size()) == (1); }, 5000));
    QSignalSpy imported(&controller, &StorageController::imported);
    auto editor = std::make_unique<ItemEditor>(controller, std::nullopt);
    editor->importPath(path);
    editor.reset();
    QVERIFY(QTest::qWaitFor([&] { return (imported.size()) == (1); }, 5000));
    QVERIFY(!controls->wrongThread);
  }
  void invalidInputKeepsFullDraft() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QVERIFY(controller.load());
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    ItemEditor editor(controller, std::nullopt);
    editor.show();
    fill(editor, QStringLiteral("\u00a0\u202f"));
    const auto before = editor.draft();
    QTest::mouseClick(child<QPushButton>(editor, "saveItem"), Qt::LeftButton);
    QVERIFY(editor.isVisible());
    QVERIFY(editor.draft() == before);
    QVERIFY(child<QLabel>(editor, "editorError")->isVisible());
    QCOMPARE(controls->writes.load(), 0);
  }
  void enterInNoteAndSaveShortcut() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    QSignalSpy saved(&controller, &StorageController::saved);
    ItemEditor editor(controller, std::nullopt);
    editor.show();
    fill(editor);
    auto* note = child<QPlainTextEdit>(editor, "itemNote");
    note->setPlainText(QStringLiteral("A"));
    note->moveCursor(QTextCursor::End);
    note->setFocus();
    QTest::keyClick(note, Qt::Key_Return);
    QTest::keyClicks(note, "B");
    QCOMPARE(note->toPlainText(), QStringLiteral("A\nB"));
    QCOMPARE(saved.size(), 0);
    QVERIFY(QTest::qWaitForWindowActive(&editor));
    QTest::keySequence(&editor, QKeySequence::Save);
    QVERIFY(QTest::qWaitFor([&] { return (saved.size()) == (1); }, 5000));
    QVERIFY(!editor.isVisible());
    QCOMPARE(qvariant_cast<Collectible>(saved.first().first()).fields.note, QStringLiteral("A\nB"));
  }
  void dirtyCloseProtection_data() {
    QTest::addColumn<int>("action");
    QTest::newRow("cancel") << 0;
    QTest::newRow("escape") << 1;
    QTest::newRow("window-close") << 2;
  }
  void dirtyCloseProtection() {
    QFETCH(int, action);
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    ItemEditor editor(controller, std::nullopt);
    editor.show();
    fill(editor);
    const auto draft = editor.draft();
    const auto close = [&] {
      if (action == 0) {
        child<QPushButton>(editor, "cancelEdit")->click();
      } else if (action == 1) {
        QTest::keyClick(&editor, Qt::Key_Escape);
      } else {
        editor.close();
      }
    };
    bool checked = false;
    answerConfirmation(QStringLiteral("discardConfirmation"), QStringLiteral("keepEditing"),
                       QStringLiteral("keepEditing"), &checked);
    close();
    QVERIFY(checked);
    QVERIFY(editor.isVisible());
    QVERIFY(editor.draft() == draft);
    checked = false;
    answerConfirmation(QStringLiteral("discardConfirmation"), QStringLiteral("discardChanges"),
                       QStringLiteral("keepEditing"), &checked);
    close();
    QVERIFY(checked);
    QVERIFY(!editor.isVisible());
    QCOMPARE(controls->writes.load(), 0);
  }
  void unchangedEditorClosesImmediately() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    ItemEditor editor(controller, Collectible{9, {QStringLiteral("Azul"), {}, {}, {}}});
    editor.show();
    child<QLineEdit>(editor, "itemName")->setText(QStringLiteral("Änderung"));
    child<QLineEdit>(editor, "itemName")->setText(QStringLiteral("Azul"));
    QVERIFY(editor.requestClose());
    QVERIFY(!editor.isVisible());
  }
  void failedSavePreservesDraftAndRetries() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    QSignalSpy failed(&controller, &StorageController::failed);
    QSignalSpy saved(&controller, &StorageController::saved);
    ItemEditor editor(controller, std::nullopt);
    editor.show();
    fill(editor);
    const auto draft = editor.draft();
    controls->failWrites = true;
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return (failed.size()) == (1); }, 5000));
    QVERIFY(editor.isVisible());
    QVERIFY(editor.draft() == draft);
    QVERIFY(child<QPushButton>(editor, "saveItem")->isEnabled());
    QVERIFY(child<QLabel>(editor, "editorError")->isVisible());
    QCOMPARE(saved.size(), 0);
    QCOMPARE(controls->writes.load(), 0);
    controls->failWrites = false;
    editor.save();
    editor.save();
    QVERIFY(QTest::qWaitFor([&] { return (saved.size()) == (1); }, 5000));
    QCOMPARE(controls->writes.load(), 1);
    QVERIFY(!editor.isVisible());
  }
  void backgroundWorkAndTeardown() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    {
      StorageController controller(factory(directory.path(), controls));
      QSignalSpy loaded(&controller, &StorageController::loaded);
      controller.load();
      QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
      auto editor = std::make_unique<ItemEditor>(controller, std::nullopt);
      editor->show();
      fill(*editor);
      controls->delay = 300;
      editor->save();
      QVERIFY(!editor->requestClose());
      QVERIFY(!controller.remove(1));
      bool responsive = false;
      QTimer::singleShot(10, [&responsive] { responsive = true; });
      QVERIFY(QTest::qWaitFor([&] { return responsive; }, 5000));
      QVERIFY(controller.isBusy());
      editor.reset(); // A late success must not call a destroyed dialog.
      QVERIFY(QTest::qWaitFor([&] { return !controller.isBusy(); }, 5000));
    }
    QVERIFY(controls->destroyed);
    QVERIFY(!controls->wrongThread);
  }
  void listSelectionFilteringAndDeletion() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    MainWindow window(controller, directory.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    QVERIFY(child<QLabel>(window, "collectionState")->text().contains(QStringLiteral("noch leer")));
    auto* add = child<QPushButton>(window, "addItem");
    add->click();
    auto* editor = window.findChild<ItemEditor*>();
    QVERIFY(editor);
    fill(*editor);
    editor->save();
    auto* table = child<QTableWidget>(window, "itemsTable");
    QVERIFY(QTest::qWaitFor([&] { return (table->rowCount()) == (1); }, 5000));
    QVERIFY(QTest::qWaitFor([&] { return add->isEnabled(); }, 5000));
    const auto id = table->item(0, 0)->data(Qt::UserRole).toLongLong();
    auto* search = child<QLineEdit>(window, "searchItems");
    search->setText(QStringLiteral("AZUL"));
    child<QPushButton>(window, "editItem")->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    editor = window.findChild<ItemEditor*>();
    QVERIFY(editor);
    child<QLineEdit>(*editor, "itemName")->setText(QStringLiteral("Catan"));
    editor->save();
    QVERIFY(QTest::qWaitFor([&] { return (table->rowCount()) == (0); }, 5000));
    QVERIFY(child<QLabel>(window, "mainFeedback")
                ->text()
                .contains(QStringLiteral("wurde gespeichert, passt aber nicht")));
    QVERIFY(child<QLabel>(window, "collectionState")
                ->text()
                .contains(QStringLiteral("Keine Sammlerstücke")));
    search->clear();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->data(Qt::UserRole).toLongLong(), id);
    QVERIFY(QTest::qWaitFor([&] { return child<QPushButton>(window, "removeItem")->isEnabled(); },
                            5000));
    bool checked = false;
    answerConfirmation(QStringLiteral("deleteConfirmation"), QStringLiteral("cancelDelete"),
                       QStringLiteral("cancelDelete"), &checked);
    child<QPushButton>(window, "removeItem")->click();
    QVERIFY(checked);
    QCOMPARE(table->rowCount(), 1);
    checked = false;
    answerConfirmation(QStringLiteral("deleteConfirmation"), QStringLiteral("confirmDelete"),
                       QStringLiteral("cancelDelete"), &checked);
    child<QPushButton>(window, "removeItem")->click();
    QVERIFY(checked);
    QVERIFY(QTest::qWaitFor([&] { return (table->rowCount()) == (0); }, 5000));
    QVERIFY(child<QLabel>(window, "collectionState")->text().contains(QStringLiteral("noch leer")));
  }
  void mainWindowProtectsDraftAndBusySave() {
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    MainWindow window(controller, directory.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return (loaded.size()) == (1); }, 5000));
    child<QPushButton>(window, "addItem")->click();
    auto* editor = window.findChild<ItemEditor*>();
    QVERIFY(editor);
    fill(*editor);
    bool checked = false;
    answerConfirmation(QStringLiteral("discardConfirmation"), QStringLiteral("keepEditing"),
                       QStringLiteral("keepEditing"), &checked);
    QVERIFY(!window.close());
    QVERIFY(checked);
    QVERIFY(editor->isVisible());
    controls->delay = 200;
    editor->save();
    QVERIFY(!window.close());
    QVERIFY(QTest::qWaitFor([&] { return !controller.isBusy(); }, 5000));
    QVERIFY(window.close());
  }
  void startupErrorIsNotAnEmptyCollection() {
    QTemporaryDir directory;
    class FailingRepository final : public ItemRepository {
    public:
      TransferDraftList transferSnapshot() override {
        throw std::logic_error("Unexpected test transfer");
      }
      CollectionImportResult importEntries(const TransferDraftList&) override {
        throw std::logic_error("Unexpected test transfer");
      }
      void initialize() override { throw StorageError(QStringLiteral("Synthetischer Pfadfehler")); }
      ItemList findAll() override { return {}; }
      Collectible find(qint64) override { return {}; }
      Collectible create(const ItemDraft&) override { return {}; }
      Collectible update(qint64, const ItemDraft&) override { return {}; }
      void remove(qint64) override {}
    };
    StorageController failing([] { return std::make_unique<FailingRepository>(); });
    QSignalSpy failed(&failing, &StorageController::failed);
    MainWindow window(failing, directory.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return (failed.size()) == (1); }, 5000));
    QVERIFY(child<QLabel>(window, "collectionState")
                ->text()
                .contains(QStringLiteral("nicht geöffnet")));
    QVERIFY(!child<QPushButton>(window, "addItem")->isEnabled());
    QVERIFY(child<QPushButton>(window, "reloadItems")->isVisible());
  }
  void discardedEditLeavesStoredOriginal() {
    QTemporaryDir directory;
    Collectible original;
    {
      SqliteItemRepository repository(directory.path());
      repository.initialize();
      original = repository.create({QStringLiteral("Original"), {}, {}, QStringLiteral("Notiz")});
    }
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }, 5000));
    ItemEditor editor(controller, original);
    editor.show();
    fill(editor, QStringLiteral("Geändert"));
    bool checked = false;
    answerConfirmation(QStringLiteral("discardConfirmation"), QStringLiteral("discardChanges"),
                       QStringLiteral("keepEditing"), &checked);
    QVERIFY(editor.requestClose());
    QVERIFY(checked);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 2; }, 5000));
    const auto items = qvariant_cast<ItemList>(loaded.last().first());
    QVERIFY(items == ItemList{original});
  }
  void failedDeletePreservesRowAndSelection() {
    QTemporaryDir directory;
    {
      SqliteItemRepository repository(directory.path());
      repository.initialize();
      repository.create({QStringLiteral("Original"), {}, {}, {}});
    }
    auto controls = std::make_shared<Controls>();
    controls->failWrites = true;
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    QSignalSpy failed(&controller, &StorageController::failed);
    MainWindow window(controller, directory.path());
    window.show();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }, 5000));
    auto* table = child<QTableWidget>(window, "itemsTable");
    const auto id = table->item(0, 0)->data(Qt::UserRole);
    bool checked = false;
    answerConfirmation(QStringLiteral("deleteConfirmation"), QStringLiteral("confirmDelete"),
                       QStringLiteral("cancelDelete"), &checked);
    child<QPushButton>(window, "removeItem")->click();
    QVERIFY(QTest::qWaitFor([&] { return failed.size() == 1; }, 5000));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->currentRow(), 0);
    QCOMPARE(table->item(0, 0)->data(Qt::UserRole), id);
    QCOMPARE(controls->writes.load(), 0);
  }
  void largeFontSmallDialogKeepsActionsReachable() {
    const auto originalFont = QApplication::font();
    const auto restoreFont = qScopeGuard([originalFont] { QApplication::setFont(originalFont); });
    auto largerFont = originalFont;
    largerFont.setPointSize(18);
    QApplication::setFont(largerFont);
    QTemporaryDir directory;
    auto controls = std::make_shared<Controls>();
    StorageController controller(factory(directory.path(), controls));
    QSignalSpy loaded(&controller, &StorageController::loaded);
    controller.load();
    QVERIFY(QTest::qWaitFor([&] { return loaded.size() == 1; }, 5000));
    ItemEditor editor(controller, std::nullopt);
    editor.resize(400, 340);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.save();
    auto* save = child<QPushButton>(editor, "saveItem");
    auto* cancel = child<QPushButton>(editor, "cancelEdit");
    for (auto* button : {save, cancel}) {
      QVERIFY(button->isVisible());
      const QRect bounds(button->mapTo(&editor, QPoint(0, 0)), button->size());
      QVERIFY(editor.rect().contains(bounds));
    }
    auto* scroll = editor.findChild<QScrollArea*>();
    QVERIFY(scroll);
    QVERIFY(scroll->widget()->height() > scroll->viewport()->height());
    const auto screenshot = qEnvironmentVariable("SAMMLERVITRINE_TEST_SCREENSHOT");
    if (!screenshot.isEmpty()) {
      QVERIFY(editor.grab().save(screenshot));
    }
  }
};

QTEST_MAIN(DialogsTest)
#include "dialogs_test.moc"
