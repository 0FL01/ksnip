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
		return mSelectedRectArea;
	}

	void setSelectedRectArea(const QRect &rect)
	{
		mSelectedRectArea = rect;
	}

	void selectRectArea(const QPointF &from, const QPointF &to)
	{
		auto selector = findChild<SnippingAreaSelector *>();
		selector->handleMousePress(from);
		selector->handleMouseMove(to);
		selector->handleMouseRelease();
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

	bool hasTransparentBackground() const
	{
		return isBackgroundTransparent();
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
	QRect mSelectedRectArea { 1, 1, 2, 2 };
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
	grabber.mRectAreaClient.imageReady(createBackground());
	QTRY_COMPARE(backgroundRequests.size(), 2);
	QVERIFY(!backgroundRequests.constLast());
	QCOMPARE(snippingArea->showCount(), 0);

	grabber.grabImage(CaptureModes::RectArea, true, 0);
	grabber.mRectAreaClient.canceled();
	QTRY_COMPARE(backgroundRequests.size(), 3);
	QVERIFY(backgroundRequests.constLast());
	QCOMPARE(canceledCount, 0);

	grabber.grabImage(CaptureModes::RectArea, false, 0);
	QTest::ignoreMessage(QtWarningMsg, "KWin ScreenShot2 RectArea background capture failed: superseded");
	grabber.mRectAreaClient.failed(QLatin1String("superseded"));
	QTRY_COMPARE(backgroundRequests.size(), 4);
	QVERIFY(!backgroundRequests.constLast());
	QCOMPARE(canceledCount, 0);

	grabber.mRectAreaClient.imageReady(createBackground());
	QCOMPARE(snippingArea->showCount(), 1);
	QVERIFY(snippingArea->isVisible());
	QVERIFY(snippingArea->isTimeoutActive());

	grabber.grabImage(CaptureModes::RectArea, true, 0);
	QVERIFY(!snippingArea->isVisible());
	QVERIFY(!snippingArea->isTimeoutActive());
	QTRY_COMPARE(backgroundRequests.size(), 5);
	QVERIFY(backgroundRequests.constLast());

	grabber.mRectAreaClient.imageReady(createBackground());
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
	grabber.mRectAreaClient.imageReady(createBackground());
	QTest::keyClick(snippingArea, Qt::Key_Escape);
	QCOMPARE(canceledCount, 1);
	QCOMPARE(finishedCount, 0);

	grabber.grabImage(CaptureModes::RectArea, true, 0);
	QTRY_COMPARE(backgroundRequests.size(), 2);
	QVERIFY(backgroundRequests.constLast());
	grabber.mRectAreaClient.imageReady(createBackground());
	snippingArea->selectRectArea({ 1, 1 }, { 3, 3 });
	QTest::keyClick(snippingArea, Qt::Key_Return);

	QCOMPARE(canceledCount, 1);
	QCOMPARE(finishedCount, 1);
	QCOMPARE(screenshot.size(), QSize(2, 2));
	QCOMPARE(screenshot.toImage().pixelColor(0, 0), QColor(Qt::red));
	QCOMPARE(screenshot.toImage().pixelColor(1, 1), QColor(Qt::yellow));
	QCOMPARE(grabber.mRectAreaState, KdeWaylandImageGrabber::RectAreaState::Idle);
}

void KdeWaylandImageGrabberTests::GrabImage_Should_CropLogicalBackground_When_PrimaryScreenDprDiffers()
{
	auto config = createConfig();
	auto snippingArea = new TestWaylandSnippingArea(config);
	snippingArea->setSelectedRectArea({ 2, 2, 4, 4 });
	int backgroundRequestCount = 0;
	KdeWaylandImageGrabber grabber(snippingArea,
									config,
									KdeWaylandImageGrabber::Backend::ScreenShot2,
									[&backgroundRequestCount](bool) { backgroundRequestCount++; });
	QPixmap screenshot;
	connect(&grabber, &IImageGrabber::finished, [&screenshot](const CaptureDto &capture) {
		screenshot = capture.screenshot;
	});

	grabber.grabImage(CaptureModes::RectArea, false, 0);
	QTRY_COMPARE(backgroundRequestCount, 1);
	grabber.mRectAreaClient.imageReady(createBackground());
	snippingArea->selectRectArea({ 1, 1 }, { 3, 3 });
	QTest::keyClick(snippingArea, Qt::Key_Return);

	QCOMPARE(screenshot.size(), QSize(2, 2));
	QCOMPARE(screenshot.toImage().pixelColor(0, 0), QColor(Qt::red));
	QCOMPARE(screenshot.toImage().pixelColor(1, 1), QColor(Qt::yellow));
}

void KdeWaylandImageGrabberTests::GrabImage_Should_ShowLiveSelectorWithoutCancel_When_RectAreaBackgroundCaptureFails()
{
	auto config = createConfig();
	auto snippingArea = new TestWaylandSnippingArea(config);
	int backgroundRequestCount = 0;
	KdeWaylandImageGrabber grabber(snippingArea,
									config,
									KdeWaylandImageGrabber::Backend::ScreenShot2,
									[&backgroundRequestCount](bool) { backgroundRequestCount++; });
	int canceledCount = 0;
	int captureAreaCount = 0;
	connect(&grabber, &IImageGrabber::canceled, [&canceledCount] {
		canceledCount++;
	});
	grabber.mCaptureRectArea = [&captureAreaCount](const QRect &, bool) {
		captureAreaCount++;
	};

	grabber.grabImage(CaptureModes::RectArea, false, 0);
	QTRY_COMPARE(backgroundRequestCount, 1);
	QTest::ignoreMessage(QtWarningMsg,
					 "KWin ScreenShot2 RectArea background capture failed; using live CaptureArea fallback: early EOF");
	grabber.mRectAreaClient.failed(QLatin1String("early EOF"));

	QCOMPARE(snippingArea->showCount(), 1);
	QVERIFY(snippingArea->isVisible());
	QVERIFY(snippingArea->hasTransparentBackground());
	QCOMPARE(canceledCount, 0);
	QCOMPARE(grabber.mRectAreaState, KdeWaylandImageGrabber::RectAreaState::SelectingFallback);

	QTest::keyClick(snippingArea, Qt::Key_Escape);
	QCOMPARE(canceledCount, 1);
	QCOMPARE(captureAreaCount, 0);
	QCOMPARE(grabber.mRectAreaState, KdeWaylandImageGrabber::RectAreaState::Idle);
}

void KdeWaylandImageGrabberTests::GrabImage_Should_QueueCaptureArea_When_LiveFallbackSelectionFinishes()
{
	auto config = createConfig();
	auto snippingArea = new TestWaylandSnippingArea(config);
	int backgroundRequestCount = 0;
	KdeWaylandImageGrabber grabber(snippingArea,
									config,
									KdeWaylandImageGrabber::Backend::ScreenShot2,
									[&backgroundRequestCount](bool) { backgroundRequestCount++; });
	QList<QPair<QRect, bool>> captureAreaRequests;
	bool selectorVisibleDuringCapture = true;
	grabber.mCaptureRectArea = [&](const QRect &area, bool captureCursor) {
		captureAreaRequests.append({ area, captureCursor });
		selectorVisibleDuringCapture = snippingArea->isVisible();
	};
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

	grabber.grabImage(CaptureModes::RectArea, true, 0);
	QTRY_COMPARE(backgroundRequestCount, 1);
	QTest::ignoreMessage(QtWarningMsg,
					 "KWin ScreenShot2 RectArea background capture failed; using live CaptureArea fallback: early EOF");
	grabber.mRectAreaClient.failed(QLatin1String("early EOF"));
	snippingArea->selectRectArea({ 1, 1 }, { 3, 3 });
	QTest::keyClick(snippingArea, Qt::Key_Return);

	QCOMPARE(captureAreaRequests.size(), 0);
	QTRY_COMPARE(captureAreaRequests.size(), 1);
	QCOMPARE(captureAreaRequests.constFirst().first, QRect(1, 1, 2, 2));
	QVERIFY(captureAreaRequests.constFirst().second);
	QVERIFY(!selectorVisibleDuringCapture);
	QCOMPARE(canceledCount, 0);
	QCOMPARE(finishedCount, 0);
	QCOMPARE(grabber.mRectAreaState, KdeWaylandImageGrabber::RectAreaState::CapturingFallback);

	auto captureImage = createBackground().copy(1, 1, 2, 2);
	grabber.mRectAreaClient.imageReady(captureImage);

	QCOMPARE(canceledCount, 0);
	QCOMPARE(finishedCount, 1);
	QCOMPARE(screenshot.size(), QSize(2, 2));
	QCOMPARE(screenshot.toImage().pixelColor(0, 0), QColor(Qt::red));
	QCOMPARE(screenshot.toImage().pixelColor(1, 1), QColor(Qt::yellow));
	QCOMPARE(grabber.mRectAreaState, KdeWaylandImageGrabber::RectAreaState::Idle);
}

TEST_MAIN(KdeWaylandImageGrabberTests)
