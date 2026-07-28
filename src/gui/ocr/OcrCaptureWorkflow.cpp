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

#include "OcrCaptureWorkflow.h"

OcrCaptureWorkflow::OcrCaptureWorkflow(const QSharedPointer<IClipboard> &clipboard, QObject *parent) :
	QObject(parent),
	mClipboard(clipboard),
	mWatcher(nullptr),
	mState(State::Idle)
{
}

bool OcrCaptureWorkflow::start(const QSharedPointer<IOcrRecognizer> &recognizer)
{
	if (mState != State::Idle || recognizer.isNull()) {
		return false;
	}

	mRecognizer = recognizer;
	mState = State::Selecting;
	emit captureRequested();
	return true;
}

bool OcrCaptureWorkflow::isCapturePending() const
{
	return mState == State::Selecting;
}

bool OcrCaptureWorkflow::isRecognizing() const
{
	return mState == State::Recognizing;
}

void OcrCaptureWorkflow::processCapture(const CaptureDto &capture)
{
	if (mState != State::Selecting) {
		emit editorCaptureReady(capture);
		return;
	}

	if (!capture.isValid()) {
		reset();
		emit captureReleased();
		emit recognitionFinished(false);
		return;
	}

	mState = State::Recognizing;
	auto image = capture.screenshot.toImage().copy();
	emit captureReleased();
	startRecognition(image);
}

void OcrCaptureWorkflow::processCancellation()
{
	if (mState == State::Selecting) {
		reset();
		emit captureReleased();
		return;
	}

	emit editorCaptureCanceled();
}

void OcrCaptureWorkflow::startRecognition(const QImage &image)
{
	auto recognizer = mRecognizer;
	auto watcher = new QFutureWatcher<OcrResult>(this);
	mWatcher = watcher;
	connect(watcher, &QFutureWatcher<OcrResult>::finished, this, [this, watcher] {
		finishRecognition(watcher);
	});

	watcher->setFuture(QtConcurrent::run([image, recognizer] {
		try {
			return recognizer->recognize(image);
		} catch (...) {
			return OcrResult{ {}, QStringLiteral("Recognition failed") };
		}
	}));
}

void OcrCaptureWorkflow::finishRecognition(QFutureWatcher<OcrResult> *watcher)
{
	if (watcher != mWatcher) {
		watcher->deleteLater();
		return;
	}

	auto result = watcher->result();
	mWatcher = nullptr;
	watcher->deleteLater();
	reset();

	auto text = normalizeText(result.text);
	auto textCopied = result.isSuccessful() && !text.isEmpty();
	if (textCopied) {
		mClipboard->setText(text);
	}
	emit recognitionFinished(textCopied);
}

void OcrCaptureWorkflow::reset()
{
	mRecognizer.clear();
	mState = State::Idle;
}

QString OcrCaptureWorkflow::normalizeText(QString text)
{
	text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
	text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
	return text.trimmed();
}
