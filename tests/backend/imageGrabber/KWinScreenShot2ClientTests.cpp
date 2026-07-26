/*
 * Copyright (C) 2017 Damir Porobic <https://github.com/damirporobic>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "KWinScreenShot2ClientTests.h"

#include "src/backend/imageGrabber/KWinScreenShot2Client.h"
#include "tests/utils/TestRunner.h"

namespace
{
QVariantMap metadata(int width, int height, int stride)
{
	return {
		{ QLatin1String("type"), QLatin1String("raw") },
		{ QLatin1String("width"), width },
		{ QLatin1String("height"), height },
		{ QLatin1String("stride"), stride },
		{ QLatin1String("format"), static_cast<int>(QImage::Format_ARGB32) },
		{ QLatin1String("scale"), 1.5 }
	};
}
}

void KWinScreenShot2ClientTests::ReadImage_Should_ReturnCompleteImage_When_MetadataAndPayloadAreValid()
{
	int pipeDescriptors[2];
	QVERIFY(::pipe(pipeDescriptors) == 0);
	QByteArray content(24, 0);
	auto firstPixel = qRgb(12, 34, 56);
	memcpy(content.data(), &firstPixel, sizeof(firstPixel));
	QCOMPARE(::write(pipeDescriptors[1], content.constData(), 7), 7);
	QCOMPARE(::write(pipeDescriptors[1], content.constData() + 7, content.size() - 7), content.size() - 7);
	::close(pipeDescriptors[1]);

	auto result = KWinScreenShot2Client::readImage(pipeDescriptors[0], metadata(2, 2, 12), 100);

	QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
	QCOMPARE(result.image.size(), QSize(2, 2));
	QCOMPARE(result.image.pixel(0, 0), firstPixel);
	QCOMPARE(result.image.devicePixelRatio(), 1.5);
}

void KWinScreenShot2ClientTests::ReadImage_Should_Fail_When_PayloadEndsEarly()
{
	int pipeDescriptors[2];
	QVERIFY(::pipe(pipeDescriptors) == 0);
	QByteArray content(4, 0);
	QCOMPARE(::write(pipeDescriptors[1], content.constData(), content.size()), content.size());
	::close(pipeDescriptors[1]);

	auto result = KWinScreenShot2Client::readImage(pipeDescriptors[0], metadata(2, 2, 12), 100);

	QVERIFY(result.image.isNull());
	QVERIFY(result.error.contains(QLatin1String("before the image was complete")));
}

void KWinScreenShot2ClientTests::ReadImage_Should_Fail_When_ReadTimesOut()
{
	int pipeDescriptors[2];
	QVERIFY(::pipe(pipeDescriptors) == 0);

	auto result = KWinScreenShot2Client::readImage(pipeDescriptors[0], metadata(2, 2, 12), 20);
	::close(pipeDescriptors[1]);

	QVERIFY(result.image.isNull());
	QVERIFY(result.error.contains(QLatin1String("Timed out")));
}

void KWinScreenShot2ClientTests::ReadImage_Should_CloseDescriptor_When_MetadataIsInvalid()
{
	int pipeDescriptors[2];
	QVERIFY(::pipe(pipeDescriptors) == 0);
	auto invalidMetadata = metadata(2, 2, 4);

	auto result = KWinScreenShot2Client::readImage(pipeDescriptors[0], invalidMetadata, 100);
	::close(pipeDescriptors[1]);

	QVERIFY(result.image.isNull());
	QVERIFY(result.error.contains(QLatin1String("stride is too small")));
	QCOMPARE(::fcntl(pipeDescriptors[0], F_GETFD), -1);
	QCOMPARE(errno, EBADF);
}

void KWinScreenShot2ClientTests::CaptureArea_Should_FailWithoutDBusCall_When_SizeIsEmpty()
{
	KWinScreenShot2Client client;
	QSignalSpy failedSpy(&client, &KWinScreenShot2Client::failed);

	client.captureArea(QRect(-100, 50, 0, 20), false);

	QCOMPARE(failedSpy.count(), 1);
}

TEST_MAIN(KWinScreenShot2ClientTests)
