/*
 * Copyright (C) 2017 Damir Porobic <https://github.com/damirporobic>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "KdeWaylandImageGrabberTests.h"

#include "src/backend/imageGrabber/KdeWaylandImageGrabber.h"
#include "tests/mocks/backend/config/ConfigMock.h"
#include "tests/utils/TestRunner.h"

namespace
{
class TestWaylandSnippingArea : public WaylandSnippingArea
{
public:
	explicit TestWaylandSnippingArea(const QSharedPointer<IConfig> &config) :
		WaylandSnippingArea(config),
		mShowCount(0)
	{
	}

	QRect selectedRectArea() const override
	{
		return { 1, 1, 2, 2 };
	}

	int showCount() const
	{
		return mShowCount;
	}

	bool isTimeoutActive() const
	{
		auto timers = findChildren<QTimer *>(QString(), Qt::FindDirectChildrenOnly);
		return !timers.isEmpty() && timers.constFirst()->isActive();
	}

protected:
	void setFullScreen() override
	{
		setGeometry(0, 0, 8, 8);
		QWidget::show();
	}

	QSizeF getSize() const override
	{
		return { 8, 8 };
	}

	void grabKeyboardFocus() override
	{
	}

	void showSnippingArea() override
	{
		mShowCount++;
		AbstractSnippingArea::showSnippingArea();
	}

private:
	int mShowCount;
};

QSharedPointer<ConfigMock> createConfig()
{
	return QSharedPointer<ConfigMock>(new testing::NiceMock<ConfigMock>);
}

QImage createBackground()
{
	QImage image(8, 8, QImage::Format_ARGB32);
	image.fill(Qt::black);
	image.setPixelColor(1, 1, Qt::red);
	image.setPixelColor(2, 1, Qt::green);
	image.setPixelColor(1, 2, Qt::blue);
	image.setPixelColor(2, 2, Qt::yellow);
	return image;
}
}

void KdeWaylandImageGrabberTests::GrabImage_Should_KeepLatestRectAreaRequest_When_ScreenShot2ProbeIsPending()
{
	auto config = createConfig();
	auto snippingArea = new TestWaylandSnippingArea(config);
	QList<bool> backgroundRequests;
	KdeWaylandImageGrabber grabber(snippingArea,
									config,
									KdeWaylandImageGrabber::Backend::Pending,
									[&backgroundRequests](bool captureCursor) {
										backgroundRequests.append(captureCursor);
									});

	grabber.grabImage(CaptureModes::RectArea, false, 0);
	grabber.grabImage(CaptureModes::RectArea, false, 0);
	grabber.grabImage(CaptureModes::RectArea, true, 0);
	grabber.mScreenShot2Client.probeFinished(true, 2, {});

	QTRY_COMPARE(backgroundRequests.size(), 1);
	QVERIFY(backgroundRequests.constFirst());
	QCOMPARE(grabber.mRectAreaState, KdeWaylandImageGrabber::RectAreaState::CapturingBackground);
}

void KdeWaylandImageGrabberTests::GrabImage_Should_ReplaceActiveRectAreaCapture_When_RequestsOverlap()
{
	auto config = createConfig();
	auto snippingArea = new TestWaylandSnippingArea(config);
	QList<bool> backgroundRequests;
	KdeWaylandImageGrabber grabber(snippingArea,
									config,
									KdeWaylandImageGrabber::Backend::ScreenShot2,
									[&backgroundRequests](bool captureCursor) {
										backgroundRequests.append(captureCursor);
									});
	int canceledCount = 0;
	connect(&grabber, &IImageGrabber::canceled, [&canceledCount] {
		canceledCount++;
	});

	grabber.grabImage(CaptureModes::RectArea, false, 1000);
	grabber.grabImage(CaptureModes::RectArea, true, 0);
	QTRY_COMPARE(backgroundRequests.size(), 1);
	QVERIFY(backgroundRequests.constFirst());

	grabber.grabImage(CaptureModes::RectArea, true, 0);
	grabber.grabImage(CaptureModes::RectArea, false, 0);
	grabber.mRectAreaBackgroundClient.imageReady(createBackground());
	QTRY_COMPARE(backgroundRequests.size(), 2);
	QVERIFY(!backgroundRequests.constLast());
	QCOMPARE(snippingArea->showCount(), 0);

	grabber.grabImage(CaptureModes::RectArea, true, 0);
	grabber.mRectAreaBackgroundClient.canceled();
	QTRY_COMPARE(backgroundRequests.size(), 3);
	QVERIFY(backgroundRequests.constLast());
	QCOMPARE(canceledCount, 0);

	grabber.grabImage(CaptureModes::RectArea, false, 0);
	QTest::ignoreMessage(QtWarningMsg, "KWin ScreenShot2 RectArea background capture failed: superseded");
	grabber.mRectAreaBackgroundClient.failed(QLatin1String("superseded"));
	QTRY_COMPARE(backgroundRequests.size(), 4);
	QVERIFY(!backgroundRequests.constLast());
	QCOMPARE(canceledCount, 0);

	grabber.mRectAreaBackgroundClient.imageReady(createBackground());
	QCOMPARE(snippingArea->showCount(), 1);
	QVERIFY(snippingArea->isVisible());
	QVERIFY(snippingArea->isTimeoutActive());

	grabber.grabImage(CaptureModes::RectArea, true, 0);
	QVERIFY(!snippingArea->isVisible());
	QVERIFY(!snippingArea->isTimeoutActive());
	QTRY_COMPARE(backgroundRequests.size(), 5);
	QVERIFY(backgroundRequests.constLast());

	grabber.mRectAreaBackgroundClient.imageReady(createBackground());
	QCOMPARE(snippingArea->showCount(), 2);
	QVERIFY(snippingArea->isVisible());
	QTest::keyClick(snippingArea, Qt::Key_Escape);

	QCOMPARE(canceledCount, 1);
	QCOMPARE(grabber.mRectAreaState, KdeWaylandImageGrabber::RectAreaState::Idle);
	QVERIFY(!snippingArea->isVisible());
	QVERIFY(!snippingArea->isTimeoutActive());
	QCoreApplication::processEvents();
	QCOMPARE(backgroundRequests.size(), 5);
	QCOMPARE(snippingArea->showCount(), 2);
}

void KdeWaylandImageGrabberTests::GrabImage_Should_StartFreshRectAreaCapture_When_PreviousSelectionWasCanceled()
{
	auto config = createConfig();
	auto snippingArea = new TestWaylandSnippingArea(config);
	QList<bool> backgroundRequests;
	KdeWaylandImageGrabber grabber(snippingArea,
									config,
									KdeWaylandImageGrabber::Backend::ScreenShot2,
									[&backgroundRequests](bool captureCursor) {
										backgroundRequests.append(captureCursor);
									});
	int canceledCount = 0;
	int finishedCount = 0;
	QPixmap screenshot;
	connect(&grabber, &IImageGrabber::canceled, [&canceledCount] {
		canceledCount++;
	});
	connect(&grabber, &IImageGrabber::finished, [&finishedCount, &screenshot](const CaptureDto &capture) {
		finishedCount++;
		screenshot = capture.screenshot;
	});

	grabber.grabImage(CaptureModes::RectArea, false, 0);
	QTRY_COMPARE(backgroundRequests.size(), 1);
	grabber.mRectAreaBackgroundClient.imageReady(createBackground());
	QTest::keyClick(snippingArea, Qt::Key_Escape);
	QCOMPARE(canceledCount, 1);
	QCOMPARE(finishedCount, 0);

	grabber.grabImage(CaptureModes::RectArea, true, 0);
	QTRY_COMPARE(backgroundRequests.size(), 2);
	QVERIFY(backgroundRequests.constLast());
	grabber.mRectAreaBackgroundClient.imageReady(createBackground());
	QTest::keyClick(snippingArea, Qt::Key_Return);

	QCOMPARE(canceledCount, 1);
	QCOMPARE(finishedCount, 1);
	QCOMPARE(screenshot.size(), QSize(2, 2));
	QCOMPARE(screenshot.toImage().pixelColor(0, 0), QColor(Qt::red));
	QCOMPARE(screenshot.toImage().pixelColor(1, 1), QColor(Qt::yellow));
	QCOMPARE(grabber.mRectAreaState, KdeWaylandImageGrabber::RectAreaState::Idle);
}

TEST_MAIN(KdeWaylandImageGrabberTests)
