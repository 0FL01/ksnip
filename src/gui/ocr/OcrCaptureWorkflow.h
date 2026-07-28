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

#ifndef KSNIP_OCRCAPTUREWORKFLOW_H
#define KSNIP_OCRCAPTUREWORKFLOW_H

#include <QFutureWatcher>
#include <QObject>
#include <QSharedPointer>
#include <QtConcurrent>

#include "src/common/dtos/CaptureDto.h"
#include "src/gui/clipboard/IClipboard.h"
#include "src/gui/ocr/IOcrRecognizer.h"

class OcrCaptureWorkflow : public QObject
{
	Q_OBJECT
public:
	explicit OcrCaptureWorkflow(const QSharedPointer<IClipboard> &clipboard, QObject *parent = nullptr);
	~OcrCaptureWorkflow() override = default;

	bool start(const QSharedPointer<IOcrRecognizer> &recognizer);
	bool isCapturePending() const;
	bool isRecognizing() const;

public slots:
	void processCapture(const CaptureDto &capture);
	void processCancellation();

signals:
	void captureRequested();
	void captureReleased();
	void editorCaptureReady(const CaptureDto &capture);
	void editorCaptureCanceled();
	void recognitionFinished(bool textCopied);

private:
	enum class State
	{
		Idle,
		Selecting,
		Recognizing
	};

	QSharedPointer<IClipboard> mClipboard;
	QSharedPointer<IOcrRecognizer> mRecognizer;
	QFutureWatcher<OcrResult> *mWatcher;
	State mState;

	void startRecognition(const QImage &image);
	void finishRecognition(QFutureWatcher<OcrResult> *watcher);
	void reset();
	static QString normalizeText(QString text);
};

#endif // KSNIP_OCRCAPTUREWORKFLOW_H
