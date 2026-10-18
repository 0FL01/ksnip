/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "RasterStoreTest.h"

namespace {

class ScopedEnvironment
{
public:
	ScopedEnvironment(const char *name, const QByteArray &value) :
		mName(name), mPrevious(qgetenv(name)), mWasSet(qEnvironmentVariableIsSet(name))
	{
		qputenv(name, value);
	}

	~ScopedEnvironment()
	{
		if (mWasSet) { qputenv(mName.constData(), mPrevious); }
		else { qunsetenv(mName.constData()); }
	}

private:
	QByteArray mName;
	QByteArray mPrevious;
	bool mWasSet;
};

}

QString RasterStoreTest::directoryTemplate()
{
	// CTest's build working directory, never the platform's possibly-RAM temp dir.
	return QDir::current().absoluteFilePath(QStringLiteral("raster-store-test-XXXXXX"));
}

QImage RasterStoreTest::pixelPattern(QImage::Format format)
{
	QImage image(13, 7, format);
	for (int y = 0; y < image.height(); ++y) {
		memset(image.scanLine(y), 0, size_t(image.bytesPerLine()));
		for (int x = 0; x < image.width(); ++x) {
			image.setPixelColor(x, y, QColor((x * 29 + y * 13 + 32) % 256,
				(x * 7 + y * 31 + 64) % 256, (x * 43 + y * 17 + 128) % 256));
		}
	}
	image.setDevicePixelRatio(1.25);
	image.setColorSpace(QColorSpace(QColorSpace::DisplayP3));
	image.setDotsPerMeterX(4211);
	image.setDotsPerMeterY(4223);
	image.setOffset(QPoint(-7, 15));
	image.setText(QStringLiteral("author"), QStringLiteral("ignored-bit regression"));
	return image;
}

QByteArray RasterStoreTest::rawDigest(const QImage &image)
{
	RasterDescriptor descriptor(image);
	QCryptographicHash hash(QCryptographicHash::Sha256);
	hash.addData(descriptor.canonical());
	for (int row = 0; row < image.height(); ++row) {
		hash.addData(reinterpret_cast<const char *>(image.constScanLine(row)), int(descriptor.rowBytes));
	}
	return hash.result();
}

void RasterStoreTest::poisonIgnoredBits(QImage &image, uchar value)
{
	const int rowBytes = int(RasterDescriptor(image).rowBytes);
	constexpr int highByte16 = Q_BYTE_ORDER == Q_LITTLE_ENDIAN ? 1 : 0;
	constexpr int highByte32 = Q_BYTE_ORDER == Q_LITTLE_ENDIAN ? 3 : 0;
	for (int y = 0; y < image.height(); ++y) {
		auto row = image.scanLine(y);
		memset(row + rowBytes, value, size_t(image.bytesPerLine() - rowBytes));
		for (int x = 0; x < image.width(); ++x) {
			switch (image.format()) {
			case QImage::Format_RGB32:
				row[x * 4 + highByte32] = value;
				break;
			case QImage::Format_RGBX8888:
				row[x * 4 + 3] = value;
				break;
			case QImage::Format_RGBX64:
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
			case QImage::Format_RGBX16FPx4:
#endif
				memset(row + x * 8 + 6, value, 2);
				break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
			case QImage::Format_RGBX32FPx4:
				memset(row + x * 16 + 12, value, 4);
				break;
#endif
			case QImage::Format_RGB30:
			case QImage::Format_BGR30:
				row[x * 4 + highByte32] = (row[x * 4 + highByte32] & 0x3f) | (value & 0xc0);
				break;
			case QImage::Format_RGB555:
				row[x * 2 + highByte16] = (row[x * 2 + highByte16] & 0x7f) | (value & 0x80);
				break;
			case QImage::Format_RGB444:
				row[x * 2 + highByte16] = (row[x * 2 + highByte16] & 0x0f) | (value & 0xf0);
				break;
			case QImage::Format_RGB666:
				row[x * 3] = (row[x * 3] & 0x03) | (value & 0xfc);
				break;
			case QImage::Format_ARGB8555_Premultiplied:
				row[x * 3] = (row[x * 3] & 0x7f) | (value & 0x80);
				break;
			default:
				Q_UNREACHABLE();
			}
		}
	}
}

bool RasterStoreTest::isMemoryBacked(const QStorageInfo &storage)
{
	auto type = storage.fileSystemType().toLower();
	return type == "tmpfs" || type == "ramfs" || type == "rootfs" || type == "devtmpfs" || type == "hugetlbfs" || type == "mfs" ||
		storage.device().startsWith("/dev/ram") || storage.device().startsWith("/dev/zram");
}

QString RasterStoreTest::memoryBackedDirectory()
{
	// Read-only discovery of an already mounted RAM filesystem; never mount one
	// or create a temporary directory there to run a regression test.
	QStorageInfo temporary(QDir::tempPath());
	if (temporary.isValid() && temporary.isReady() && isMemoryBacked(temporary)) { return QDir::tempPath(); }
	for (const auto &storage : QStorageInfo::mountedVolumes()) {
		if (storage.isValid() && storage.isReady() && isMemoryBacked(storage) &&
			QFileInfo(storage.rootPath()).isDir() && !QFileInfo(storage.rootPath()).canonicalFilePath().isEmpty()) {
			return storage.rootPath();
		}
	}
	return {};
}

QImage RasterStoreTest::pattern(uchar padding)
{
	QImage image(13, 9, QImage::Format_RGB888);
	for (int y = 0; y < image.height(); ++y) {
		memset(image.scanLine(y), padding, size_t(image.bytesPerLine()));
		for (int x = 0; x < image.width(); ++x) {
			image.setPixelColor(x, y, QColor((x * 29 + y * 13) % 256, (x * 7 + y * 31) % 256, (x * 43 + y) % 256));
		}
	}
	image.setDevicePixelRatio(1.75);
	image.setColorSpace(QColorSpace(QColorSpace::DisplayP3));
	image.setDotsPerMeterX(4211);
	image.setDotsPerMeterY(4223);
	image.setOffset(QPoint(-7, 15));
	image.setText(QStringLiteral("author"), QStringLiteral("pixel test"));
	return image;
}

void RasterStoreTest::Prepare_Should_DeduplicateMeaningfulRowsAndRestoreMetadata()
{
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	RasterStore store;
	QString error;
	QVERIFY(store.initialize(directory.path(), &error));
	RasterDocument firstDocument, secondDocument;
	RasterImage first(pattern(0x13), &firstDocument), second(pattern(0xd7), &secondDocument);
	auto firstHandle = first.handle();
	auto secondHandle = second.handle();
	auto metadata = firstHandle->descriptor.canonical();
	bool done = false;
	store.prepare({firstHandle, secondHandle}, false, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY2(error.isEmpty(), qPrintable(error));
	QVERIFY(firstHandle->blob);
	QCOMPARE(firstHandle->blob, secondHandle->blob);
	QCOMPARE(QFileInfo(firstHandle->blob->path).size(), qint64(13 * 3 * 9));
	QCOMPARE(QDir(store.sessionDirectory()).entryList(QDir::Files).size(), 1);
	QCOMPARE(QFile::permissions(firstHandle->blob->path) & (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup |
		QFile::ReadOther | QFile::WriteOther | QFile::ExeOther), QFile::Permissions());
	QCOMPARE(QFile::permissions(store.sessionDirectory()) & (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup |
		QFile::ReadOther | QFile::WriteOther | QFile::ExeOther), QFile::Permissions());
	firstDocument.setResident(false);
	secondDocument.setResident(false);
	QVERIFY(first.image().isNull());
	QVERIFY(second.image().isNull());
	firstDocument.setResident(true);
	done = false;
	store.prepare({firstHandle}, true, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY2(error.isEmpty(), qPrintable(error));
	QCOMPARE(RasterDescriptor(first.image()).canonical(), metadata);
	QCOMPARE(first.image(), pattern(0));
	QVERIFY(second.image().isNull());
}

void RasterStoreTest::Prepare_Should_CompareCandidatesAndRejectCorruptReads()
{
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	RasterStore store;
	QString error;
	QVERIFY(store.initialize(directory.path(), &error));
	RasterDocument document;
	RasterImage first(pattern(0), &document);
	bool done = false;
	store.prepare({first.handle()}, false, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY(error.isEmpty());
	auto corruptPath = first.handle()->blob->path;
	QFile corrupt(corruptPath);
	QVERIFY(corrupt.open(QIODevice::ReadWrite));
	QCOMPARE(corrupt.write("bad", 3), qint64(3));
	corrupt.close();
	RasterImage second(pattern(0), &document);
	done = false;
	store.prepare({second.handle()}, false, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY(error.isEmpty());
	// A matching digest is only a candidate, not proof of equality.
	QVERIFY(first.handle()->blob != second.handle()->blob);
	document.setResident(false);
	QVERIFY(first.image().isNull());
	document.setResident(true);
	done = false;
	store.prepare({first.handle(), second.handle()}, true, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY(!error.isEmpty());
	QVERIFY(first.image().isNull());
	QVERIFY(second.image().isNull()); // Decode batches publish atomically.
}

void RasterStoreTest::Prepare_Should_KeepLastDecodedCopy_When_WriteFails()
{
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	RasterStore store;
	QString error;
	QVERIFY(store.initialize(directory.path(), &error));
	RasterDocument document;
	RasterImage raster(pattern(0), &document);
	auto original = store.sessionDirectory();
	auto displaced = original + QStringLiteral("-away");
	QVERIFY(QDir().rename(original, displaced));
	bool done = false;
	store.prepare({raster.handle()}, false, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY(!error.isEmpty());
	QVERIFY(!raster.handle()->blob);
	document.setResident(false);
	QCOMPARE(raster.image(), pattern(0));
	QVERIFY(QDir().rename(displaced, original));
}

void RasterStoreTest::Lifetime_Should_RemoveBlobAndSessionAfterLastConsumer()
{
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	QString session, path;
	{
		RasterStore store;
		QString error;
		QVERIFY(store.initialize(directory.path(), &error));
		session = store.sessionDirectory();
		RasterDocument document;
		RasterImage raster(pattern(0), &document);
		bool done = false;
		store.prepare({raster.handle()}, false, [&](const QString &message) { error = message; done = true; });
		QTRY_VERIFY(done);
		QVERIFY(error.isEmpty());
		path = raster.handle()->blob->path;
		QVERIFY(QFile::exists(path));
		raster = QImage();
		QTRY_VERIFY(!QFile::exists(path));
	}
	QTRY_VERIFY(!QDir(session).exists());
}

void RasterStoreTest::Prepare_Should_RejectInvalidDescriptorWithoutPublishingPixels()
{
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	RasterStore store;
	QString error;
	QVERIFY(store.initialize(directory.path(), &error));
	RasterImage raster(pattern(0));
	raster.handle()->descriptor.byteCount += 1;
	bool done = false;
	store.prepare({raster.handle()}, true, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY(!error.isEmpty());
	QVERIFY(!raster.handle()->blob);
	QCOMPARE(raster.image(), pattern(0));
}

void RasterStoreTest::Prepare_Should_PreservePaletteAndIgnoreUnusedMonoBits_data()
{
	QTest::addColumn<int>("format");
	QTest::newRow("Mono") << int(QImage::Format_Mono);
	QTest::newRow("MonoLSB") << int(QImage::Format_MonoLSB);
}

void RasterStoreTest::Prepare_Should_PreservePaletteAndIgnoreUnusedMonoBits()
{
	QFETCH(int, format);
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	RasterStore store;
	QString error;
	QVERIFY(store.initialize(directory.path(), &error));
	RasterDocument document;
	QImage first(9, 7, QImage::Format(format));
	first.setColorTable({qRgb(5, 10, 15), qRgb(90, 120, 150)});
	first.fill(1);
	auto second = first.copy();
	for (int y = 0; y < second.height(); ++y) {
		first.scanLine(y)[1] = format == int(QImage::Format_Mono) ? 0x80 : 0x01;
		second.scanLine(y)[1] = 0xff; // Same ninth pixel, different unused bits.
	}
	RasterImage a(first, &document), b(second, &document);
	first = QImage();
	second = QImage();
	bool done = false;
	store.prepare({a.handle(), b.handle()}, false, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY(error.isEmpty());
	QCOMPARE(a.handle()->blob, b.handle()->blob);
	QCOMPARE(QFileInfo(a.handle()->blob->path).size(), qint64(14));
	document.setResident(false);
	QVERIFY(a.image().isNull());
	document.setResident(true);
	done = false;
	store.prepare({a.handle()}, true, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY(error.isEmpty());
	QCOMPARE(a.image().colorTable(), QVector<QRgb>({qRgb(5, 10, 15), qRgb(90, 120, 150)}));
	for (int y = 0; y < a.image().height(); ++y) {
		for (int x = 0; x < a.image().width(); ++x) { QCOMPARE(a.image().pixelIndex(x, y), 1); }
	}
}

void RasterStoreTest::Prepare_Should_NotMergeEqualPixelsWithDifferentMetadata()
{
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	RasterStore store;
	QString error;
	QVERIFY(store.initialize(directory.path(), &error));
	RasterDocument document;
	QList<RasterImage> rasters;
	auto image = pattern(0);
	rasters.append(RasterImage(image, &document));
	image.setDevicePixelRatio(2.0);
	rasters.append(RasterImage(image, &document));
	image = pattern(0);
	image.setColorSpace(QColorSpace(QColorSpace::SRgb));
	rasters.append(RasterImage(image, &document));
	image = pattern(0);
	image.setDotsPerMeterX(8000);
	rasters.append(RasterImage(image, &document));
	image = pattern(0);
	image.setDotsPerMeterY(9000);
	rasters.append(RasterImage(image, &document));
	image = pattern(0);
	image.setOffset(QPoint(7, -15));
	rasters.append(RasterImage(image, &document));
	image = pattern(0);
	image.setText(QStringLiteral("author"), QStringLiteral("other metadata"));
	rasters.append(RasterImage(image, &document));
	image = pattern(0).convertToFormat(QImage::Format_RGB32);
	rasters.append(RasterImage(image, &document));
	image = QImage();
	bool done = false;
	store.prepare(document.handles(), false, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY(error.isEmpty());
	QSet<RasterBlob *> blobs;
	QSet<QByteArray> digests;
	for (const auto &raster : qAsConst(rasters)) {
		QVERIFY(raster.handle()->blob);
		blobs.insert(raster.handle()->blob.data());
		digests.insert(raster.handle()->blob->digest);
	}
	QCOMPARE(blobs.size(), rasters.size());
	QCOMPARE(digests.size(), rasters.size());
	document.setResident(false);
	for (const auto &raster : qAsConst(rasters)) { QVERIFY(raster.image().isNull()); }
}

void RasterStoreTest::Lifetime_Should_KeepBlobUntilConsumerOutlivesStore()
{
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	RasterImage consumer;
	QString path, session;
	{
		RasterStore store;
		QString error;
		QVERIFY(store.initialize(directory.path(), &error));
		session = store.sessionDirectory();
		consumer = pattern(0);
		bool done = false;
		store.prepare({consumer.handle()}, false, [&](const QString &message) { error = message; done = true; });
		QTRY_VERIFY(done);
		QVERIFY(error.isEmpty());
		path = consumer.handle()->blob->path;
	}
	QVERIFY(QFile::exists(path));
	consumer = QImage();
	QTRY_VERIFY(!QFile::exists(path));
	QTRY_VERIFY(!QDir(session).exists());
}

void RasterStoreTest::Prepare_Should_DeduplicateIgnoredPixelBits_data()
{
	QTest::addColumn<int>("format");
	QTest::newRow("RGB32") << int(QImage::Format_RGB32);
	QTest::newRow("RGBX8888") << int(QImage::Format_RGBX8888);
	QTest::newRow("RGBX64") << int(QImage::Format_RGBX64);
	QTest::newRow("RGB30") << int(QImage::Format_RGB30);
	QTest::newRow("BGR30") << int(QImage::Format_BGR30);
	QTest::newRow("RGB555") << int(QImage::Format_RGB555);
	QTest::newRow("RGB444") << int(QImage::Format_RGB444);
	QTest::newRow("RGB666") << int(QImage::Format_RGB666);
	QTest::newRow("ARGB8555_Premultiplied") << int(QImage::Format_ARGB8555_Premultiplied);
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
	QTest::newRow("RGBX16FPx4") << int(QImage::Format_RGBX16FPx4);
	QTest::newRow("RGBX32FPx4") << int(QImage::Format_RGBX32FPx4);
#endif
}

void RasterStoreTest::Prepare_Should_DeduplicateIgnoredPixelBits()
{
	QFETCH(int, format);
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	RasterStore store;
	QString error;
	QVERIFY2(store.initialize(directory.path(), &error), qPrintable(error));
	RasterDocument firstDocument, secondDocument;
	auto first = pixelPattern(QImage::Format(format));
	auto second = pixelPattern(QImage::Format(format)); // Independent storage, not an implicitly shared copy.
	QVERIFY(!first.isNull());
	QVERIFY(!second.isNull());
	QVERIFY(first.constBits() != second.constBits());
	auto expected = rawDigest(first); // Qt-generated valid opaque X values and zero packed padding bits.
	auto expectedMetadata = RasterDescriptor(first).canonical();
	auto expectedColor = first.pixelColor(7, 3);
	poisonIgnoredBits(first, 0x00);
	poisonIgnoredBits(second, 0xff);
	auto firstRaw = rawDigest(first);
	auto secondRaw = rawDigest(second);
	QVERIFY(firstRaw != secondRaw);
	RasterImage a(first, &firstDocument), b(second, &secondDocument);
	first = QImage();
	second = QImage(); // Release test-owned rasters before testing cold residency.
	bool done = false;
	store.prepare({a.handle(), b.handle()}, false, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY2(error.isEmpty(), qPrintable(error));
	QVERIFY(a.handle()->blob);
	QCOMPARE(a.handle()->blob, b.handle()->blob);
	QCOMPARE(a.handle()->blob->digest, expected);
	QCOMPARE(QDir(store.sessionDirectory()).entryList(QDir::Files).size(), 1);
	QCOMPARE(QFileInfo(a.handle()->blob->path).size(), a.handle()->descriptor.byteCount);
	QCOMPARE(rawDigest(a.image()), firstRaw); // Canonicalization must not mutate either current source.
	QCOMPARE(rawDigest(b.image()), secondRaw);
	firstDocument.setResident(false);
	secondDocument.setResident(false);
	QVERIFY(a.image().isNull());
	QVERIFY(b.image().isNull());
	firstDocument.setResident(true);
	secondDocument.setResident(true);
	done = false;
	store.prepare({a.handle(), b.handle()}, true, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY2(error.isEmpty(), qPrintable(error));
	QCOMPARE(a.image().format(), QImage::Format(format));
	QCOMPARE(b.image().format(), QImage::Format(format));
	QCOMPARE(RasterDescriptor(a.image()).canonical(), expectedMetadata);
	QCOMPARE(RasterDescriptor(b.image()).canonical(), expectedMetadata);
	QCOMPARE(rawDigest(a.image()), expected);
	QCOMPARE(rawDigest(b.image()), expected);
	QCOMPARE(a.image(), b.image());
	QCOMPARE(a.image().pixelColor(7, 3), expectedColor);
}

void RasterStoreTest::Prepare_Should_NotDiscardRealAlpha_data()
{
	QTest::addColumn<int>("format");
	QTest::newRow("ARGB32") << int(QImage::Format_ARGB32);
	QTest::newRow("RGBA8888") << int(QImage::Format_RGBA8888);
	QTest::newRow("RGBA64") << int(QImage::Format_RGBA64);
	QTest::newRow("ARGB8555_Premultiplied") << int(QImage::Format_ARGB8555_Premultiplied);
	QTest::newRow("ARGB8565_Premultiplied") << int(QImage::Format_ARGB8565_Premultiplied);
	QTest::newRow("ARGB6666_Premultiplied") << int(QImage::Format_ARGB6666_Premultiplied);
	QTest::newRow("ARGB4444_Premultiplied") << int(QImage::Format_ARGB4444_Premultiplied);
	QTest::newRow("A2RGB30_Premultiplied") << int(QImage::Format_A2RGB30_Premultiplied);
	QTest::newRow("A2BGR30_Premultiplied") << int(QImage::Format_A2BGR30_Premultiplied);
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
	QTest::newRow("RGBA16FPx4") << int(QImage::Format_RGBA16FPx4);
	QTest::newRow("RGBA32FPx4") << int(QImage::Format_RGBA32FPx4);
#endif
}

void RasterStoreTest::Prepare_Should_NotDiscardRealAlpha()
{
	QFETCH(int, format);
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	RasterStore store;
	QString error;
	QVERIFY2(store.initialize(directory.path(), &error), qPrintable(error));
	RasterDocument document;
	QImage first(13, 7, QImage::Format(format)), second(13, 7, QImage::Format(format));
	QVERIFY(!first.isNull());
	QVERIFY(!second.isNull());
	first.fill(QColor(10, 20, 30, 255));
	second.fill(QColor(10, 20, 30, 127)); // Real alpha and any premultiplied RGB must remain significant.
	auto expectedFirst = rawDigest(first);
	auto expectedSecond = rawDigest(second);
	QVERIFY(expectedFirst != expectedSecond);
	RasterImage a(first, &document), b(second, &document);
	first = QImage();
	second = QImage();
	bool done = false;
	store.prepare({a.handle(), b.handle()}, false, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY2(error.isEmpty(), qPrintable(error));
	QVERIFY(a.handle()->blob != b.handle()->blob);
	QCOMPARE(a.handle()->blob->digest, expectedFirst);
	QCOMPARE(b.handle()->blob->digest, expectedSecond);
	document.setResident(false);
	QVERIFY(a.image().isNull());
	QVERIFY(b.image().isNull());
	document.setResident(true);
	done = false;
	store.prepare({a.handle(), b.handle()}, true, [&](const QString &message) { error = message; done = true; });
	QTRY_VERIFY(done);
	QVERIFY2(error.isEmpty(), qPrintable(error));
	QCOMPARE(rawDigest(a.image()), expectedFirst);
	QCOMPARE(rawDigest(b.image()), expectedSecond);
}

void RasterStoreTest::Initialize_Should_RejectMemoryBackedFilesystems_data()
{
	QTest::addColumn<QString>("path");
	QTest::addColumn<QByteArray>("type");
	auto volumes = QStorageInfo::mountedVolumes();
	volumes.prepend(QStorageInfo(QDir::tempPath()));
	QSet<QByteArray> seen;
	for (const auto &storage : qAsConst(volumes)) {
		auto type = storage.fileSystemType().toLower();
		if (storage.isValid() && storage.isReady() && isMemoryBacked(storage) && !seen.contains(type) &&
			QFileInfo(storage.rootPath()).isDir() && !QFileInfo(storage.rootPath()).canonicalFilePath().isEmpty()) {
			seen.insert(type);
			QTest::newRow(type.constData()) << storage.rootPath() << type;
		}
	}
	if (seen.isEmpty()) { QSKIP("No existing memory-backed filesystem is available for read-only probing."); }
}

void RasterStoreTest::Initialize_Should_RejectMemoryBackedFilesystems()
{
	QFETCH(QString, path);
	QFETCH(QByteArray, type);
	RasterStore store;
	QString error;
	QVERIFY(!store.initialize(path, &error));
	QVERIFY2(error.contains(QStringLiteral("memory-backed")), qPrintable(error));
	QVERIFY(error.contains(QString::fromLatin1(type)));
	QVERIFY(store.sessionDirectory().isEmpty());
	QVERIFY(!store.isBusy());
}

void RasterStoreTest::Initialize_Should_RejectSymlinkToMemoryBackedFilesystem()
{
#ifdef Q_OS_UNIX
	auto memory = memoryBackedDirectory();
	if (memory.isEmpty()) { QSKIP("No existing memory-backed filesystem is available for read-only probing."); }
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	auto link = QDir(directory.path()).filePath(QStringLiteral("ram-link"));
	QVERIFY(QFile::link(memory, link)); // Only the build-local symlink is created, not a file on RAM storage.
	RasterStore store;
	QString error;
	auto missing = QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral("/captures");
	QVERIFY(!store.initialize(QDir(link).filePath(missing), &error));
	QVERIFY2(error.contains(QStringLiteral("memory-backed")), qPrintable(error));
	QVERIFY(store.sessionDirectory().isEmpty());
#else
	QSKIP("Directory symlink regression uses Unix filesystem semantics.");
#endif
}

void RasterStoreTest::Initialize_Should_UseOneDiskBackedDefaultFallback()
{
#ifdef Q_OS_LINUX
	auto memory = memoryBackedDirectory();
	if (memory.isEmpty()) { QSKIP("No existing memory-backed filesystem is available for read-only probing."); }
	QTemporaryDir directory(directoryTemplate());
	QVERIFY(directory.isValid());
	auto absent = QDir(memory).filePath(QStringLiteral("raster-store-uncreated-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
	ScopedEnvironment cache("XDG_CACHE_HOME", QFile::encodeName(absent));
	ScopedEnvironment data("XDG_DATA_HOME", QFile::encodeName(QDir(directory.path()).filePath(QStringLiteral("data"))));
	auto fallback = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
		.filePath(QStringLiteral("cache/captures"));
	QVERIFY(fallback.startsWith(directory.path() + QLatin1Char('/')));
	QString session;
	{
		RasterStore store;
		QString error;
		QVERIFY2(store.initialize(QString(), &error), qPrintable(error));
		QVERIFY(error.isEmpty());
		session = store.sessionDirectory();
		QVERIFY(session.startsWith(fallback + QLatin1Char('/')));
		QStorageInfo actual(session);
		QVERIFY(actual.isValid() && actual.isReady());
		QVERIFY(!isMemoryBacked(actual));
		RasterDocument document;
		RasterImage raster(pattern(0), &document);
		bool done = false;
		store.prepare({raster.handle()}, false, [&](const QString &message) { error = message; done = true; });
		QTRY_VERIFY(done);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		QVERIFY(raster.handle()->blob);
		QVERIFY(raster.handle()->blob->path.startsWith(session + QLatin1Char('/')));
		QCOMPARE(QFileInfo(raster.handle()->blob->path).size(), raster.handle()->descriptor.byteCount);
		QStorageInfo fileStorage(raster.handle()->blob->path);
		QVERIFY(fileStorage.isValid() && fileStorage.isReady());
		QVERIFY(!isMemoryBacked(fileStorage));
		QVERIFY(!QFileInfo(absent).exists());
	}
	QTRY_VERIFY(!QDir(session).exists());
#else
	QSKIP("Default filesystem fallback is tested with Linux XDG environment paths.");
#endif
}

void RasterStoreTest::Initialize_Should_FailWhenBothDefaultLocationsAreMemoryBacked()
{
#ifdef Q_OS_LINUX
	auto memory = memoryBackedDirectory();
	if (memory.isEmpty()) { QSKIP("No existing memory-backed filesystem is available for read-only probing."); }
	auto absent = QDir(memory).filePath(QStringLiteral("raster-store-uncreated-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
	ScopedEnvironment cache("XDG_CACHE_HOME", QFile::encodeName(absent));
	ScopedEnvironment data("XDG_DATA_HOME", QFile::encodeName(absent));
	RasterStore store;
	QString error;
	QVERIFY(!store.initialize(QString(), &error));
	QVERIFY2(error.contains(QStringLiteral("memory-backed")), qPrintable(error));
	QVERIFY(store.sessionDirectory().isEmpty());
	QVERIFY(!store.isBusy());
	QVERIFY(!QFileInfo(absent).exists());
#else
	QSKIP("Default filesystem fallback is tested with Linux XDG environment paths.");
#endif
}

TEST_MAIN(RasterStoreTest);
