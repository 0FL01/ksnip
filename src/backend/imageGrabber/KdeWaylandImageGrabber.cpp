/*
 * Copyright (C) 2017 Damir Porobic <https://github.com/damirporobic>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "KdeWaylandImageGrabber.h"

KdeWaylandImageGrabber::KdeWaylandImageGrabber(const QSharedPointer<IConfig> &config) :
	KdeWaylandImageGrabber(new WaylandSnippingArea(config), config)
{
}

KdeWaylandImageGrabber::KdeWaylandImageGrabber(WaylandSnippingArea *snippingArea,
																					const QSharedPointer<IConfig> &config) :
	AbstractRectAreaImageGrabber(snippingArea, config),
	mSnippingArea(snippingArea),
	mPortalGrabber(config),
	mBackend(Backend::Pending),
	mPortalBusy(false)
{
	addSupportedCaptureMode(CaptureModes::RectArea);
	addSupportedCaptureMode(CaptureModes::ActiveWindow);
	addSupportedCaptureMode(CaptureModes::WindowUnderCursor);
	addSupportedCaptureMode(CaptureModes::CurrentScreen);
	addSupportedCaptureMode(CaptureModes::FullScreen);

	connect(&mScreenShot2Client, &KWinScreenShot2Client::probeFinished, this,
			[this](bool available, uint version, const QString &error) {
				if (available) {
					qInfo("Using KWin ScreenShot2 API version %u", version);
					mBackend = Backend::ScreenShot2;
				} else {
					qWarning("KWin ScreenShot2 is unavailable, using the Screenshot portal: %s",
							 qPrintable(error));
					mBackend = Backend::Portal;
				}

				auto requests = mDeferredCaptures;
				mDeferredCaptures.clear();
				for (const auto &request : requests) {
					grabImage(request.mode, request.captureCursor, request.delay);
				}
			});
	connect(&mScreenShot2Client, &KWinScreenShot2Client::imageReady, this, [this](const QImage &image) {
		auto pixmap = QPixmap::fromImage(image);
		if (pixmap.isNull()) {
			qWarning("Failed to create a pixmap from the ScreenShot2 image");
			emit canceled();
			return;
		}
		emit finished(CaptureDto(pixmap));
	});
	connect(&mScreenShot2Client, &KWinScreenShot2Client::canceled, this, &KdeWaylandImageGrabber::canceled);
	connect(&mScreenShot2Client, &KWinScreenShot2Client::failed, this, [this](const QString &error) {
		qWarning("KWin ScreenShot2 capture failed: %s", qPrintable(error));
		emit canceled();
	});
	connect(&mPortalGrabber, &WaylandImageGrabber::finished, this, [this](const CaptureDto &capture) {
		portalRequestFinished();
		emit finished(capture);
	});
	connect(&mPortalGrabber, &WaylandImageGrabber::canceled, this, [this] {
		portalRequestFinished();
		emit canceled();
	});

	mScreenShot2Client.probe();
}

void KdeWaylandImageGrabber::grabImage(CaptureModes captureMode, bool captureCursor, int delay)
{
	if (mBackend == Backend::Pending) {
		mDeferredCaptures.append({ captureMode, captureCursor, delay });
		return;
	}

	if (captureMode == CaptureModes::RectArea && mBackend == Backend::ScreenShot2) {
		AbstractRectAreaImageGrabber::grabImage(captureMode, captureCursor, delay);
	} else {
		AbstractImageGrabber::grabImage(captureMode, captureCursor, delay);
	}
}

void KdeWaylandImageGrabber::grab()
{
	CaptureRequest request { captureMode(), isCaptureCursorEnabled(), QRect() };
	if (request.mode == CaptureModes::RectArea) {
		request.area = mSnippingArea->selectedLogicalRectArea();
		if (!request.area.isValid()) {
			emit canceled();
			return;
		}
	}

	dispatch(request);
}

void KdeWaylandImageGrabber::dispatch(const CaptureRequest &request)
{
	if (mBackend == Backend::ScreenShot2) {
		dispatchScreenShot2(request);
	} else {
		queuePortal(request);
	}
}

void KdeWaylandImageGrabber::dispatchScreenShot2(const CaptureRequest &request)
{
	switch (request.mode) {
	case CaptureModes::FullScreen:
		mScreenShot2Client.captureWorkspace(request.captureCursor);
		break;
	case CaptureModes::CurrentScreen:
		mScreenShot2Client.captureActiveScreen(request.captureCursor);
		break;
	case CaptureModes::ActiveWindow:
		mScreenShot2Client.captureActiveWindow(request.captureCursor);
		break;
	case CaptureModes::WindowUnderCursor:
		mScreenShot2Client.captureInteractive(request.captureCursor);
		break;
	case CaptureModes::RectArea:
		mScreenShot2Client.captureArea(request.area, request.captureCursor);
		break;
	default:
		qWarning("Unsupported KWin ScreenShot2 capture mode");
		emit canceled();
		break;
	}
}

QRect KdeWaylandImageGrabber::fullScreenRect() const
{
	auto primaryScreen = QGuiApplication::primaryScreen();
	return primaryScreen == nullptr ? QRect() : primaryScreen->virtualGeometry();
}

QRect KdeWaylandImageGrabber::activeWindowRect() const
{
	return {};
}

bool KdeWaylandImageGrabber::isSnippingAreaBackgroundTransparent() const
{
	return true;
}

CursorDto KdeWaylandImageGrabber::getCursorWithPosition() const
{
	return {};
}

void KdeWaylandImageGrabber::queuePortal(const CaptureRequest &request)
{
	mPortalRequests.append(request);
	processNextPortalRequest();
}

void KdeWaylandImageGrabber::processNextPortalRequest()
{
	if (mPortalBusy || mPortalRequests.isEmpty()) {
		return;
	}

	mPortalBusy = true;
	auto request = mPortalRequests.takeFirst();
	mPortalGrabber.grabImage(request.mode, request.captureCursor, 0);
}

void KdeWaylandImageGrabber::portalRequestFinished()
{
	mPortalBusy = false;
	QTimer::singleShot(0, this, [this] {
		processNextPortalRequest();
	});
}
