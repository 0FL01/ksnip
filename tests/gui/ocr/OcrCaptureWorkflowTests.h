/*
 * Copyright (C) 2026 Damir Porobic <damir.porobic@gmx.com>
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

#ifndef KSNIP_OCRCAPTUREWORKFLOWTESTS_H
#define KSNIP_OCRCAPTUREWORKFLOWTESTS_H

#include <QtTest>

class OcrCaptureWorkflowTests : public QObject
{
	Q_OBJECT
private slots:
	void Start_Should_RequestCapture_When_Idle();
	void Start_Should_RejectRequest_When_CapturePending();
	void ProcessCapture_Should_CopyNormalizedText_When_OcrCaptureSucceeds();
	void ProcessCapture_Should_PreserveClipboard_When_ResultIsEmpty();
	void ProcessCapture_Should_PreserveClipboard_When_RecognitionFails();
	void ProcessCancellation_Should_PreserveClipboard_When_OcrCapturePending();
	void ProcessCapture_Should_RouteCaptureToEditor_When_OcrIsIdle();
	void ProcessCapture_Should_RouteCaptureToEditor_When_RecognitionIsRunning();
};

#endif // KSNIP_OCRCAPTUREWORKFLOWTESTS_H
