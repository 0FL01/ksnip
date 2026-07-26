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

#ifndef KSNIP_KWINSCREENSHOT2CLIENT_H
#define KSNIP_KWINSCREENSHOT2CLIENT_H

#include <QtConcurrent/QtConcurrent>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusUnixFileDescriptor>
#include <QtDBus/QDBusVariant>
#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QRect>
#include <QSharedPointer>
#include <QVariantMap>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <poll.h>
#include <unistd.h>

class KWinScreenShot2Client : public QObject
{
	Q_OBJECT
public:
	struct ReadResult
	{
		QImage image;
		QString error;
	};

	explicit KWinScreenShot2Client(QObject *parent = nullptr);
	~KWinScreenShot2Client() override = default;

	void probe();
	void captureWorkspace(bool captureCursor);
	void captureActiveScreen(bool captureCursor);
	void captureActiveWindow(bool captureCursor);
	void captureInteractive(bool captureCursor);
	void captureArea(const QRect &area, bool captureCursor);

	static ReadResult readImage(int pipeFd, const QVariantMap &metadata, int timeoutMs);

signals:
	void probeFinished(bool available, uint version, const QString &error);
	void imageReady(const QImage &image);
	void canceled();
	void failed(const QString &error);

private:
	class PipeDescriptor;

	bool mProbeStarted;

	void capture(const QString &method, const QVariantList &arguments, int timeoutMs);
	void processCaptureReply(QDBusPendingCallWatcher *watcher,
							 const QSharedPointer<PipeDescriptor> &readPipe,
							 int readTimeoutMs);
	void readImageAsync(int pipeFd, const QVariantMap &metadata, int timeoutMs);
	static QVariantMap createOptions(bool captureCursor);
};

#endif // KSNIP_KWINSCREENSHOT2CLIENT_H
