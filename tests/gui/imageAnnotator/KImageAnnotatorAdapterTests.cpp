/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "KImageAnnotatorAdapterTests.h"

void KImageAnnotatorAdapterTests::Backing_Should_FollowTheActualLibraryCapability()
{
	QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/adapter-backing-XXXXXX"));
	QVERIFY(directory.isValid());
	KImageAnnotatorAdapter adapter;
	QVERIFY(!adapter.rasterBackingEnabled());
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	QVERIFY(adapter.enableRasterBacking(directory.path()));
	QVERIFY(adapter.rasterBackingEnabled());
#else
	QVERIFY(!adapter.enableRasterBacking(directory.path()));
	QVERIFY(!adapter.rasterBackingEnabled());
#endif
	QVERIFY(adapter.canAcceptImage());
}

void KImageAnnotatorAdapterTests::Capture_Should_BeSynchronouslyExportableIncludingCursor_When_BackingIsPending()
{
	QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/adapter-capture-XXXXXX"));
	QVERIFY(directory.isValid());
	KImageAnnotatorAdapter adapter;
	adapter.enableRasterBacking(directory.path());
	QPixmap background(64, 48);
	background.fill(Qt::blue);
	adapter.addTab(background, QStringLiteral("Capture"), QString());
	QPixmap cursor(3, 3);
	cursor.fill(Qt::red);
	adapter.insertImageItem(QPointF(10, 10), cursor);

	QVERIFY(adapter.isCurrentImageReady());
	const auto exported = adapter.image();
	QVERIFY(!exported.isNull());
	QCOMPARE(exported.pixelColor(1, 1), QColor(Qt::blue));
	QCOMPARE(exported.pixelColor(11, 11), QColor(Qt::red));
	QTRY_VERIFY_WITH_TIMEOUT(adapter.canAcceptImage(), 10000);
}

void KImageAnnotatorAdapterTests::RequestImageAt_Should_ForwardACompleteImageWithoutSwitchingTabs()
{
	QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/adapter-export-XXXXXX"));
	QVERIFY(directory.isValid());
	KImageAnnotatorAdapter adapter;
	adapter.enableRasterBacking(directory.path());
	QPixmap first(64, 48);
	first.fill(Qt::green);
	adapter.addTab(first, QStringLiteral("First"), QString());
	QTRY_VERIFY_WITH_TIMEOUT(adapter.canAcceptImage(), 10000);
	QPixmap second(64, 48);
	second.fill(Qt::blue);
	adapter.addTab(second, QStringLiteral("Second"), QString());
	QTRY_VERIFY_WITH_TIMEOUT(adapter.canAcceptImage(), 10000);
	QSignalSpy tabs(&adapter, &IImageAnnotator::currentTabChanged);
	QSignalSpy completion(&adapter, &IImageAnnotator::imageRequestFinished);

	adapter.requestImageAt(0, 71);
	QCOMPARE(completion.count(), 0); // The terminal result is never reentrant.
	QTRY_COMPARE_WITH_TIMEOUT(completion.count(), 1, 10000);
	const auto result = completion.first();
	QCOMPARE(result.at(0).toULongLong(), quint64(71));
	QVERIFY(!result.at(2).toBool());
	QVERIFY2(result.at(3).toString().isEmpty(), qPrintable(result.at(3).toString()));
	const auto image = qvariant_cast<QImage>(result.at(1));
	QVERIFY(!image.isNull());
	QCOMPARE(image.pixelColor(1, 1), QColor(Qt::green));
	QCOMPARE(tabs.count(), 0);
	QCOMPARE(adapter.image().pixelColor(1, 1), QColor(Qt::blue));
}

void KImageAnnotatorAdapterTests::Backing_Should_RejectAcquisition_When_InitializationFails()
{
	QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/adapter-failure-XXXXXX"));
	QVERIFY(directory.isValid());
	const auto path = directory.filePath(QStringLiteral("not-a-directory"));
	QFile file(path);
	QVERIFY(file.open(QIODevice::WriteOnly));
	file.close();
	KImageAnnotatorAdapter adapter;
	QVERIFY(!adapter.enableRasterBacking(path));
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	QVERIFY(!adapter.canAcceptImage());
#else
	QVERIFY(adapter.canAcceptImage()); // An old library has no backing feature.
#endif
}

void KImageAnnotatorAdapterTests::Shutdown_Should_CompleteAfterBackingFilesAreRemoved()
{
	QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/adapter-shutdown-XXXXXX"));
	QVERIFY(directory.isValid());
	KImageAnnotatorAdapter adapter;
	adapter.enableRasterBacking(directory.path());
	QPixmap image(64, 48);
	image.fill(Qt::blue);
	adapter.addTab(image, QStringLiteral("Capture"), QString());
	bool finished = false;
	adapter.closeRasterBacking([&finished]() { finished = true; });
	QVERIFY(!finished);
	QTRY_VERIFY_WITH_TIMEOUT(finished, 10000);
	QVERIFY(QDir(directory.path()).entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot).isEmpty());
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	QVERIFY(!adapter.canAcceptImage());
#endif
}

TEST_MAIN(KImageAnnotatorAdapterTests)
