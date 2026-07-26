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

#ifndef KSNIP_KDEWAYLANDIMAGEGRABBER_H
#define KSNIP_KDEWAYLANDIMAGEGRABBER_H

#include <QList>
#include <QTimer>

#include "AbstractRectAreaImageGrabber.h"
#include "KWinScreenShot2Client.h"
#include "WaylandImageGrabber.h"
#include "src/gui/snippingArea/WaylandSnippingArea.h"

class KdeWaylandImageGrabber : public AbstractRectAreaImageGrabber
{
public:
	explicit KdeWaylandImageGrabber(const QSharedPointer<IConfig> &config);
	~KdeWaylandImageGrabber() override = default;
	void grabImage(CaptureModes captureMode, bool captureCursor, int delay) override;
	QRect fullScreenRect() const override;
	QRect activeWindowRect() const override;

protected:
	void grab() override;
	bool isSnippingAreaBackgroundTransparent() const override;
	CursorDto getCursorWithPosition() const override;

private:
	enum class Backend
	{
		Pending,
		ScreenShot2,
		Portal
	};

	struct CaptureRequest
	{
		CaptureModes mode;
		bool captureCursor;
		QRect area;
	};

	struct DeferredCapture
	{
		CaptureModes mode;
		bool captureCursor;
		int delay;
	};

	WaylandSnippingArea *mSnippingArea;
	KWinScreenShot2Client mScreenShot2Client;
	KWinScreenShot2Client mRectAreaBackgroundClient;
	WaylandImageGrabber mPortalGrabber;
	Backend mBackend;
	QList<DeferredCapture> mDeferredCaptures;
	QList<DeferredCapture> mRectAreaCaptures;
	QList<CaptureRequest> mPortalRequests;
	bool mRectAreaCaptureActive;
	bool mPortalBusy;

	KdeWaylandImageGrabber(WaylandSnippingArea *snippingArea, const QSharedPointer<IConfig> &config);
	void queueRectAreaCapture(bool captureCursor, int delay);
	void processNextRectAreaCapture();
	void finishRectAreaSelection();
	void rectAreaCaptureFinished();
	void dispatch(const CaptureRequest &request);
	void dispatchScreenShot2(const CaptureRequest &request);
	void queuePortal(const CaptureRequest &request);
	void processNextPortalRequest();
	void portalRequestFinished();
};

#endif // KSNIP_KDEWAYLANDIMAGEGRABBER_H
