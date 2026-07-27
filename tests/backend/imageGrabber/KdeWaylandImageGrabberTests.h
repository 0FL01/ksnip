/*
 * Copyright (C) 2017 Damir Porobic <https://github.com/damirporobic>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef KSNIP_KDEWAYLANDIMAGEGRABBERTESTS_H
#define KSNIP_KDEWAYLANDIMAGEGRABBERTESTS_H

#include <QtTest>

class KdeWaylandImageGrabberTests : public QObject
{
	Q_OBJECT
private slots:
	void GrabImage_Should_KeepLatestRectAreaRequest_When_ScreenShot2ProbeIsPending();
	void GrabImage_Should_ReplaceActiveRectAreaCapture_When_RequestsOverlap();
	void GrabImage_Should_StartFreshRectAreaCapture_When_PreviousSelectionWasCanceled();
};

#endif // KSNIP_KDEWAYLANDIMAGEGRABBERTESTS_H
