#include "application/photo_importer.h"
#include <QBuffer>
#include <QFile>
#include <QImageWriter>
#include <QTemporaryDir>
#include <QtTest>

using namespace vitrine;
namespace {
QByteArray picture(const char* format, QSize size = {32, 16}) {
  QImage image(size, QImage::Format_RGB32);
  image.fill(QColor(42, 120, 130));
  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  if (!image.save(&buffer, format)) {
    return {};
  }
  return bytes;
}
} // namespace
class PhotoTest final : public QObject {
  Q_OBJECT
private slots:
  void supportedContent_data() {
    QTest::addColumn<QByteArray>("format");
    QTest::newRow("png-disguised-as-jpeg") << QByteArray("PNG");
    QTest::newRow("jpeg-disguised-as-png") << QByteArray("JPEG");
  }
  void supportedContent() {
    QFETCH(QByteArray, format);
    QTemporaryDir dir;
    const auto bytes = picture(format.constData());
    QVERIFY(!bytes.isEmpty());
    QFile file(dir.filePath(format == "PNG" ? "foto.jpg" : "foto.png"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
    file.close();
    const auto photo = importPhoto(file.fileName());
    QCOMPARE(photo.bytes, bytes);
    QCOMPARE(photo.preview.width(), photo.preview.height() * 2);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), bytes);
  }
  void invalidContent_data() {
    QTest::addColumn<QByteArray>("bytes");
    QTest::newRow("empty") << QByteArray();
    QTest::newRow("text") << QByteArray("kein Foto");
    QTest::newRow("bmp") << picture("BMP");
    auto png = picture("PNG");
    QTest::newRow("truncated-png") << png.first(png.size() - 12);
    png[45] = static_cast<char>(png.at(45) ^ 0x20);
    QTest::newRow("png-crc-error") << png;
    const auto jpeg = picture("JPEG");
    QTest::newRow("truncated-jpeg") << jpeg.first(jpeg.size() - 10);
    QTest::newRow("damaged-jpeg-with-end-marker")
        << (jpeg.first(jpeg.size() - 10) + QByteArray::fromHex("ffd9"));
    QTest::newRow("over-byte-limit") << QByteArray(maxPhotoBytes + 1, 'x');
  }
  void invalidContent() {
    QFETCH(QByteArray, bytes);
    QVERIFY_EXCEPTION_THROWN(decodePhoto(bytes), PhotoError);
  }
  void dimensionLimits_data() {
    QTest::addColumn<QSize>("size");
    QTest::addColumn<bool>("valid");
    QTest::newRow("side-at-limit") << QSize(6000, 1) << true;
    QTest::newRow("width-over") << QSize(6001, 1) << false;
    QTest::newRow("height-over") << QSize(1, 6001) << false;
    QTest::newRow("pixels-at-limit") << QSize(4000, 4000) << true;
    QTest::newRow("pixels-over") << QSize(4001, 4000) << false;
  }
  void dimensionLimits() {
    QFETCH(QSize, size);
    QFETCH(bool, valid);
    const auto bytes = picture("PNG", size);
    if (valid) {
      QVERIFY(!decodePhoto(bytes).preview.isNull());
    } else {
      QVERIFY_EXCEPTION_THROWN(decodePhoto(bytes), PhotoError);
    }
  }
  void boundedStreamDoesNotTrustInitialSize() {
    class GrowingFile final : public QIODevice {
    public:
      GrowingFile() { open(ReadOnly); }
      bool isSequential() const override { return true; }
      qint64 readData(char* data, qint64 maximum) override {
        const qint64 count = qMin(maximum, maxPhotoBytes + 100 - consumed);
        memset(data, 'x', static_cast<size_t>(count));
        consumed += count;
        return count;
      }
      qint64 writeData(const char*, qint64) override { return -1; }
      qint64 consumed = 0;
    } stream;
    QVERIFY_EXCEPTION_THROWN(readPhotoBytes(stream), PhotoError);
    // QIODevice may prefetch a bounded internal buffer, never the whole source.
    QVERIFY(stream.consumed <= maxPhotoBytes + 100);
    QByteArray exact(maxPhotoBytes, 'x');
    QBuffer buffer(&exact);
    buffer.open(QIODevice::ReadOnly);
    QCOMPARE(readPhotoBytes(buffer).size(), maxPhotoBytes);
  }
  void unreadableAndOversizedFile() {
    QTemporaryDir dir;
    QVERIFY_EXCEPTION_THROWN(importPhoto(dir.filePath("missing.png")), PhotoError);
    QVERIFY_EXCEPTION_THROWN(importPhoto(dir.path()), PhotoError);
    QFile file(dir.filePath("oversize.png"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.resize(maxPhotoBytes + 1));
    file.close();
    QVERIFY_EXCEPTION_THROWN(importPhoto(file.fileName()), PhotoError);
  }
  void exifOrientation() {
    auto jpeg = picture("JPEG");
    // APP1, Exif, little-endian TIFF, one IFD entry: orientation=6 (90 degrees).
    const QByteArray exif = QByteArray::fromHex(
        "ffe1002245786966000049492a0008000000010012010300010000000600000000000000");
    jpeg.insert(2, exif);
    const auto photo = decodePhoto(jpeg);
    QCOMPARE(photo.preview.height(), photo.preview.width() * 2);
  }
};
QTEST_GUILESS_MAIN(PhotoTest)
#include "photo_test.moc"
