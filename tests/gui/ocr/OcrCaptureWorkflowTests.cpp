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

#include "OcrCaptureWorkflowTests.h"

#include <gmock/gmock.h>

#include "src/gui/ocr/OcrCaptureWorkflow.h"
#include "tests/mocks/gui/clipboard/ClipboardMock.h"
#include "tests/utils/TestRunner.h"

class OcrRecognizerMock : public IOcrRecognizer
{
public:
	MOCK_METHOD(OcrResult, recognize, (const QImage &image), (override));
};

namespace
{
CaptureDto createCapture()
{
	QPixmap screenshot(32, 24);
	screenshot.fill(Qt::white);
	return CaptureDto(screenshot);
}
}

void OcrCaptureWorkflowTests::Start_Should_RequestCapture_When_Idle()
{
	auto clipboard = QSharedPointer<ClipboardMock>::create();
	auto recognizer = QSharedPointer<OcrRecognizerMock>::create();
	OcrCaptureWorkflow workflow(clipboard);
	QSignalSpy captureSpy(&workflow, &OcrCaptureWorkflow::captureRequested);

	auto started = workflow.start(recognizer);

	QVERIFY(started);
	QCOMPARE(captureSpy.count(), 1);
	QVERIFY(workflow.isCapturePending());
}

void OcrCaptureWorkflowTests::Start_Should_RejectRequest_When_CapturePending()
{
	auto clipboard = QSharedPointer<ClipboardMock>::create();
	auto recognizer = QSharedPointer<OcrRecognizerMock>::create();
	OcrCaptureWorkflow workflow(clipboard);
	QSignalSpy captureSpy(&workflow, &OcrCaptureWorkflow::captureRequested);

	QVERIFY(workflow.start(recognizer));
	auto secondStarted = workflow.start(recognizer);

	QVERIFY(!secondStarted);
	QCOMPARE(captureSpy.count(), 1);
}

void OcrCaptureWorkflowTests::ProcessCapture_Should_CopyNormalizedText_When_OcrCaptureSucceeds()
{
	auto clipboard = QSharedPointer<ClipboardMock>::create();
	auto recognizer = QSharedPointer<OcrRecognizerMock>::create();
	OcrCaptureWorkflow workflow(clipboard);
	QSignalSpy finishedSpy(&workflow, &OcrCaptureWorkflow::recognitionFinished);
	int editorCaptures = 0;
	connect(&workflow, &OcrCaptureWorkflow::editorCaptureReady, this, [&editorCaptures](const CaptureDto &) {
		editorCaptures++;
	});
	QThread *recognizerThread = nullptr;
	EXPECT_CALL(*recognizer, recognize(testing::_)).WillOnce(testing::Invoke([&recognizerThread](const QImage &image) {
		recognizerThread = QThread::currentThread();
		return image.isNull()
				? OcrResult{ {}, QStringLiteral("Invalid image") }
				: OcrResult{ QStringLiteral("\r\n  Привет\r\nHello  \r"), {} };
	}));
	EXPECT_CALL(*clipboard, setText(QStringLiteral("Привет\nHello"))).Times(1);

	QVERIFY(workflow.start(recognizer));
	workflow.processCapture(createCapture());

	QTRY_COMPARE(finishedSpy.count(), 1);
	QCOMPARE(editorCaptures, 0);
	QVERIFY(recognizerThread != QThread::currentThread());
	QVERIFY(!workflow.isRecognizing());
}

void OcrCaptureWorkflowTests::ProcessCapture_Should_PreserveClipboard_When_ResultIsEmpty()
{
	auto clipboard = QSharedPointer<ClipboardMock>::create();
	auto recognizer = QSharedPointer<OcrRecognizerMock>::create();
	OcrCaptureWorkflow workflow(clipboard);
	QSignalSpy finishedSpy(&workflow, &OcrCaptureWorkflow::recognitionFinished);
	EXPECT_CALL(*recognizer, recognize(testing::_)).WillOnce(testing::Return(OcrResult{ QStringLiteral(" \r\n\t"), {} }));
	EXPECT_CALL(*clipboard, setText(testing::_)).Times(0);

	QVERIFY(workflow.start(recognizer));
	workflow.processCapture(createCapture());

	QTRY_COMPARE(finishedSpy.count(), 1);
	QCOMPARE(finishedSpy.at(0).at(0).toBool(), false);
}

void OcrCaptureWorkflowTests::ProcessCapture_Should_PreserveClipboard_When_RecognitionFails()
{
	auto clipboard = QSharedPointer<ClipboardMock>::create();
	auto recognizer = QSharedPointer<OcrRecognizerMock>::create();
	OcrCaptureWorkflow workflow(clipboard);
	QSignalSpy finishedSpy(&workflow, &OcrCaptureWorkflow::recognitionFinished);
	EXPECT_CALL(*recognizer, recognize(testing::_)).WillOnce(testing::Return(OcrResult{ {}, QStringLiteral("Failed") }));
	EXPECT_CALL(*clipboard, setText(testing::_)).Times(0);

	QVERIFY(workflow.start(recognizer));
	workflow.processCapture(createCapture());

	QTRY_COMPARE(finishedSpy.count(), 1);
	QCOMPARE(finishedSpy.at(0).at(0).toBool(), false);
}

void OcrCaptureWorkflowTests::ProcessCancellation_Should_PreserveClipboard_When_OcrCapturePending()
{
	auto clipboard = QSharedPointer<ClipboardMock>::create();
	auto recognizer = QSharedPointer<OcrRecognizerMock>::create();
	OcrCaptureWorkflow workflow(clipboard);
	QSignalSpy releasedSpy(&workflow, &OcrCaptureWorkflow::captureReleased);
	QSignalSpy editorCanceledSpy(&workflow, &OcrCaptureWorkflow::editorCaptureCanceled);
	EXPECT_CALL(*recognizer, recognize(testing::_)).Times(0);
	EXPECT_CALL(*clipboard, setText(testing::_)).Times(0);

	QVERIFY(workflow.start(recognizer));
	workflow.processCancellation();

	QCOMPARE(releasedSpy.count(), 1);
	QCOMPARE(editorCanceledSpy.count(), 0);
	QVERIFY(!workflow.isCapturePending());
}

void OcrCaptureWorkflowTests::ProcessCapture_Should_RouteCaptureToEditor_When_OcrIsIdle()
{
	auto clipboard = QSharedPointer<ClipboardMock>::create();
	OcrCaptureWorkflow workflow(clipboard);
	int editorCaptures = 0;
	connect(&workflow, &OcrCaptureWorkflow::editorCaptureReady, this, [&editorCaptures](const CaptureDto &) {
		editorCaptures++;
	});

	workflow.processCapture(createCapture());

	QCOMPARE(editorCaptures, 1);
}

void OcrCaptureWorkflowTests::ProcessCapture_Should_RouteCaptureToEditor_When_RecognitionIsRunning()
{
	auto clipboard = QSharedPointer<ClipboardMock>::create();
	auto recognizer = QSharedPointer<OcrRecognizerMock>::create();
	OcrCaptureWorkflow workflow(clipboard);
	QSignalSpy finishedSpy(&workflow, &OcrCaptureWorkflow::recognitionFinished);
	int editorCaptures = 0;
	connect(&workflow, &OcrCaptureWorkflow::editorCaptureReady, this, [&editorCaptures](const CaptureDto &) {
		editorCaptures++;
	});
	EXPECT_CALL(*recognizer, recognize(testing::_)).WillOnce(testing::Return(OcrResult{ QStringLiteral("text"), {} }));
	EXPECT_CALL(*clipboard, setText(QStringLiteral("text"))).Times(1);

	QVERIFY(workflow.start(recognizer));
	workflow.processCapture(createCapture());
	QVERIFY(workflow.isRecognizing());
	workflow.processCapture(createCapture());

	QCOMPARE(editorCaptures, 1);
	QTRY_COMPARE(finishedSpy.count(), 1);
}

TEST_MAIN(OcrCaptureWorkflowTests)
