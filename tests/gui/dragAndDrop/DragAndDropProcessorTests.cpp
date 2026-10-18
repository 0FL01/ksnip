/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DragAndDropProcessorTests.h"

void DragAndDropProcessorTests::Drop_Should_RejectTemporaryInputBeforeDecode_When_AdmissionIsDenied()
{
	DragAndDropProcessor processor(nullptr, {}, [] { return false; });
	QSignalSpy images(&processor, &DragAndDropProcessor::imageDropped);
	QSignalSpy paths(&processor, &DragAndDropProcessor::temporaryFileDropped);
	QSignalSpy rejected(&processor, &DragAndDropProcessor::imageInputRejected);
	QMimeData mime;
	mime.setUrls({QUrl::fromLocalFile(QDir::tempPath() + QStringLiteral("/ksnip-no-decode.png"))});
	QDropEvent event({}, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);

	QVERIFY(processor.eventFilter(nullptr, &event));
	QVERIFY(!event.isAccepted());
	QCOMPARE(images.count(), 0);
	QCOMPARE(paths.count(), 0);
	QCOMPARE(rejected.count(), 1);
}

void DragAndDropProcessorTests::Drop_Should_PassBorrowedTemporaryPathSynchronously_When_AdmissionIsAvailable()
{
	DragAndDropProcessor processor(nullptr, {}, [] { return true; });
	const auto path = QDir::tempPath() + QStringLiteral("/ksnip-borrowed.png");
	QString received;
	connect(&processor, &DragAndDropProcessor::temporaryFileDropped, this, [&received](const QString &value) { received = value; });
	QSignalSpy images(&processor, &DragAndDropProcessor::imageDropped);
	QMimeData mime;
	mime.setUrls({QUrl::fromLocalFile(path)});
	QDropEvent event({}, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);

	QVERIFY(processor.eventFilter(nullptr, &event));
	QVERIFY(event.isAccepted());
	QCOMPARE(received, path);
	QCOMPARE(images.count(), 0);
}

void DragAndDropProcessorTests::Drop_Should_RecheckAdmissionBetweenTemporaryInputs()
{
	bool available = true;
	DragAndDropProcessor processor(nullptr, {}, [&available] { return available; });
	connect(&processor, &DragAndDropProcessor::temporaryFileDropped, this, [&available] { available = false; });
	QSignalSpy paths(&processor, &DragAndDropProcessor::temporaryFileDropped);
	QSignalSpy rejected(&processor, &DragAndDropProcessor::imageInputRejected);
	QMimeData mime;
	mime.setUrls({QUrl::fromLocalFile(QDir::tempPath() + QStringLiteral("/ksnip-first.png")),
		QUrl::fromLocalFile(QDir::tempPath() + QStringLiteral("/ksnip-second.png"))});
	QDropEvent event({}, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);

	QVERIFY(processor.eventFilter(nullptr, &event));
	QCOMPARE(paths.count(), 1);
	QCOMPARE(rejected.count(), 1);
}

void DragAndDropProcessorTests::Drop_Should_PassOrdinaryPathsWithoutDecoding_When_AdmissionIsBusy()
{
	DragAndDropProcessor processor(nullptr, {}, [] { return false; });
	QSignalSpy files(&processor, &DragAndDropProcessor::fileDropped);
	QSignalSpy images(&processor, &DragAndDropProcessor::imageDropped);
	QMimeData mime;
	const auto path = QDir::homePath() + QStringLiteral("/Pictures/ksnip-input.png");
	mime.setUrls({QUrl::fromLocalFile(path)});
	QDropEvent event({}, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);

	QVERIFY(processor.eventFilter(nullptr, &event));
	QVERIFY(event.isAccepted());
	QCOMPARE(files.count(), 1);
	QCOMPARE(files.first().first().toString(), path);
	QCOMPARE(images.count(), 0);
}

TEST_MAIN(DragAndDropProcessorTests)
