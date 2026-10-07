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

void KWinScreenShot2ClientTests::ReadImage_Should_ReturnCompleteImage_When_BufferedKWinWriterUsesSocket_data()
{
	QTest::addColumn<int>("bytes");
	QTest::newRow("four-byte-tail-over-small-pipe") << 8196;
	QTest::newRow("below-qfile-buffer") << 16380;
	QTest::newRow("qfile-buffer") << 16384;
	QTest::newRow("above-qfile-buffer") << 16388;
	QTest::newRow("large-frame") << 33177600;
	QTest::newRow("large-frame-plus-four") << 33177604;
}

void KWinScreenShot2ClientTests::ReadImage_Should_ReturnCompleteImage_When_BufferedKWinWriterUsesSocket()
{
	QFETCH(int, bytes);
	int descriptors[2];
	QVERIFY(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, descriptors) == 0);
	// QFile keeps the descriptor open; close only after its destructor flushes.
	auto closeDescriptor = [](int *fd) { ::close(*fd); delete fd; };
	std::unique_ptr<int, decltype(closeDescriptor)> reader(new int(descriptors[0]), closeDescriptor);
	std::unique_ptr<int, decltype(closeDescriptor)> writer(new int(descriptors[1]), closeDescriptor);
	int capacity = 0;
	socklen_t length = sizeof(capacity);
	QVERIFY(::getsockopt(*writer, SOL_SOCKET, SO_SNDBUF, &capacity, &length) == 0);
	QVERIFY(capacity > 16384);
	if (bytes > 16388) {
		QVERIFY(bytes > capacity);
	}
	QVERIFY(::fcntl(*reader, F_GETFD) & FD_CLOEXEC);
	QVERIFY(::fcntl(*writer, F_GETFD) & FD_CLOEXEC);
	QVERIFY(!(::fcntl(*reader, F_GETFL) & O_NONBLOCK));
	QImage image(bytes / 4, 1, QImage::Format_ARGB32);
	for (int i = 0; i < bytes; ++i) {
		image.bits()[i] = (i * 31 + i / 257) & 255;
	}
	QSemaphore firstWrite;
	auto producer = std::async(std::launch::async, [&, writerOwner = std::move(writer)]() mutable {
		auto owner = std::move(writerOwner);
		// Loop adapted from KWin v6.7.3 screenshotdbusinterface2.cpp:
		// Copyright 2010 Martin Gräßlin <mgraesslin@kde.org>,
		// 2021 Méven Car <meven.car@enioka.com>,
		// 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>.
		// SPDX-License-Identifier: GPL-2.0-or-later
		const auto fd = *owner;
		const auto flags = ::fcntl(fd, F_GETFL, 0);
		if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
			firstWrite.release();
			return qint64(-1);
		}
		QFile file;
		if (!file.open(fd, QIODevice::WriteOnly)) {
			firstWrite.release();
			return qint64(-1);
		}
		qint64 remaining = image.sizeInBytes();
		bool first = true;
		pollfd descriptor { fd, POLLOUT, 0 };
		while (true) {
			const auto ready = ::poll(&descriptor, 1, 5000);
			if (ready < 0 && errno == EINTR) {
				continue;
			}
			if (ready <= 0 || !(descriptor.revents & POLLOUT)) {
				if (first) firstWrite.release();
				return qint64(-1);
			}
			const auto written = file.write(reinterpret_cast<const char *>(image.constBits())
					+ image.sizeInBytes() - remaining, remaining);
			if (first) {
				firstWrite.release();
				first = false;
			}
			if (written < 0) return qint64(-1);
			remaining -= written;
			if (written == 0 || remaining == 0) return image.sizeInBytes() - remaining;
		}
	});
	// No reader races the first write. Small cases flush and close before reading;
	// large cases necessarily exceed capacity and resume with a concurrent reader.
	firstWrite.acquire();
	if (bytes <= 16388) producer.wait();
	const auto fd = *reader;
	delete reader.release();
	// readImage takes descriptor ownership, including failure paths.
	auto result = KWinScreenShot2Client::readImage(fd, metadata(image.width(), 1, bytes), 5000);
	QCOMPARE(producer.get(), qint64(bytes));
	QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
	QCOMPARE(result.image.sizeInBytes(), image.sizeInBytes());
	QVERIFY(memcmp(result.image.constBits(), image.constBits(), bytes) == 0);
	QCOMPARE(::fcntl(fd, F_GETFD), -1);
	QCOMPARE(errno, EBADF);
}

void KWinScreenShot2ClientTests::ReadImage_Should_Fail_When_PayloadEndsEarly_data()
{
	QTest::addColumn<int>("receivedBytes");
	QTest::newRow("empty") << 0;
	QTest::newRow("partial-pixel") << 3;
	QTest::newRow("partial-row") << 4;
	QTest::newRow("missing-final-byte") << 23;
}

void KWinScreenShot2ClientTests::ReadImage_Should_Fail_When_PayloadEndsEarly()
{
	QFETCH(int, receivedBytes);
	int pipeDescriptors[2];
	QVERIFY(::pipe(pipeDescriptors) == 0);
	QByteArray content(receivedBytes, 0);
	QCOMPARE(::write(pipeDescriptors[1], content.constData(), content.size()), content.size());
	::close(pipeDescriptors[1]);

	auto result = KWinScreenShot2Client::readImage(pipeDescriptors[0], metadata(2, 2, 12), 100);

	QVERIFY(result.image.isNull());
	QVERIFY(result.error.contains(QLatin1String("before the image was complete")));
	QVERIFY2(result.error.contains(QStringLiteral("expected_bytes=24 received_bytes=%1 width=2 height=2 stride=12 format=%2 scale=1.5")
			.arg(receivedBytes).arg(static_cast<int>(QImage::Format_ARGB32))), qPrintable(result.error));
	QCOMPARE(::fcntl(pipeDescriptors[0], F_GETFD), -1);
	QCOMPARE(errno, EBADF);
}

void KWinScreenShot2ClientTests::ReadImage_Should_Fail_When_ReadTimesOut()
{
	int pipeDescriptors[2];
	QVERIFY(::pipe(pipeDescriptors) == 0);

	auto result = KWinScreenShot2Client::readImage(pipeDescriptors[0], metadata(2, 2, 12), 20);
	::close(pipeDescriptors[1]);

	QVERIFY(result.image.isNull());
	QVERIFY(result.error.contains(QLatin1String("Timed out")));
	QVERIFY(result.error.contains(QLatin1String("expected_bytes=24 received_bytes=0")));
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
