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

/*
 * The ScreenShot2 transport is based on KDE Spectacle's ImagePlatformKWin,
 * Copyright (C) 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>.
 */

#include "KWinScreenShot2Client.h"

namespace
{
const auto sKWinService = QLatin1String("org.kde.KWin.ScreenShot2");
const auto sKWinPath = QLatin1String("/org/kde/KWin/ScreenShot2");
const auto sKWinInterface = QLatin1String("org.kde.KWin.ScreenShot2");
const auto sPropertiesInterface = QLatin1String("org.freedesktop.DBus.Properties");
const auto sCancellationError = QLatin1String("org.kde.KWin.ScreenShot2.Error.Cancelled");
constexpr int sDefaultTimeoutMs = 4000;
constexpr int sInteractiveTimeoutMs = 60000;
constexpr uint sMinimumVersion = 3;

bool readInteger(const QVariant &value, qint64 &result)
{
	switch (value.userType()) {
	case QMetaType::Int:
		result = value.toInt();
		return true;
	case QMetaType::UInt:
		result = value.toUInt();
		return true;
	case QMetaType::LongLong:
		result = value.toLongLong();
		return true;
	case QMetaType::ULongLong: {
		auto unsignedValue = value.toULongLong();
		if (unsignedValue > static_cast<qulonglong>(std::numeric_limits<qint64>::max())) {
			return false;
		}
		result = static_cast<qint64>(unsignedValue);
		return true;
	}
	default:
		return false;
	}
}

bool readScale(const QVariant &value, qreal &result)
{
	if (value.userType() != QMetaType::Double && value.userType() != QMetaType::Float) {
		return false;
	}

	bool ok;
	result = value.toDouble(&ok);
	return ok && std::isfinite(result) && result > 0;
}

KWinScreenShot2Client::ReadResult errorResult(const QString &error)
{
	return { QImage(), error };
}
}

class KWinScreenShot2Client::PipeDescriptor
{
public:
	explicit PipeDescriptor(int descriptor) :
		mDescriptor(descriptor)
	{
	}

	~PipeDescriptor()
	{
		if (mDescriptor >= 0) {
			::close(mDescriptor);
		}
	}

	int release()
	{
		auto descriptor = mDescriptor;
		mDescriptor = -1;
		return descriptor;
	}

private:
	int mDescriptor;
};

KWinScreenShot2Client::KWinScreenShot2Client(QObject *parent) :
	QObject(parent),
	mProbeStarted(false)
{
}

void KWinScreenShot2Client::probe()
{
	if (mProbeStarted) {
		return;
	}
	mProbeStarted = true;

	auto message = QDBusMessage::createMethodCall(sKWinService,
												sKWinPath,
												sPropertiesInterface,
												QLatin1String("Get"));
	message << sKWinInterface << QLatin1String("Version");
	auto pendingCall = QDBusConnection::sessionBus().asyncCall(message, sDefaultTimeoutMs);
	auto watcher = new QDBusPendingCallWatcher(pendingCall, this);
	connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
		QDBusPendingReply<QDBusVariant> reply = *watcher;
		watcher->deleteLater();
		if (reply.isError()) {
			emit probeFinished(false, 0, reply.error().message());
			return;
		}

		auto versionValue = reply.value().variant();
		if (versionValue.userType() != QMetaType::UInt) {
			emit probeFinished(false, 0, QLatin1String("ScreenShot2 Version is not an unsigned integer"));
			return;
		}

		auto version = versionValue.toUInt();
		if (version < sMinimumVersion) {
			emit probeFinished(false,
							 version,
							 QStringLiteral("ScreenShot2 version %1 is below required version %2")
									 .arg(version)
									 .arg(sMinimumVersion));
			return;
		}

		emit probeFinished(true, version, QString());
	});
}

void KWinScreenShot2Client::captureWorkspace(bool captureCursor)
{
	capture(QLatin1String("CaptureWorkspace"), { createOptions(captureCursor) }, sDefaultTimeoutMs);
}

void KWinScreenShot2Client::captureActiveScreen(bool captureCursor)
{
	capture(QLatin1String("CaptureActiveScreen"), { createOptions(captureCursor) }, sDefaultTimeoutMs);
}

void KWinScreenShot2Client::captureActiveWindow(bool captureCursor)
{
	capture(QLatin1String("CaptureActiveWindow"), { createOptions(captureCursor) }, sDefaultTimeoutMs);
}

void KWinScreenShot2Client::captureInteractive(bool captureCursor)
{
	capture(QLatin1String("CaptureInteractive"),
			{ QVariant::fromValue<quint32>(0), createOptions(captureCursor) },
			sInteractiveTimeoutMs);
}

void KWinScreenShot2Client::captureArea(const QRect &area, bool captureCursor)
{
	if (!area.isValid()) {
		emit failed(QLatin1String("Cannot capture an empty ScreenShot2 area"));
		return;
	}

	capture(QLatin1String("CaptureArea"),
			{ area.x(),
			  area.y(),
			  QVariant::fromValue<quint32>(area.width()),
			  QVariant::fromValue<quint32>(area.height()),
			  createOptions(captureCursor) },
			sDefaultTimeoutMs);
}

void KWinScreenShot2Client::capture(const QString &method, const QVariantList &arguments, int timeoutMs)
{
	// Shared across clients so background and live-area requests have distinct IDs.
	static std::atomic<quint64> requestId { 0 };
	auto context = QStringLiteral("method=%1 request=%2 pid=%3 timeout_ms=%4")
			.arg(method).arg(++requestId).arg(::getpid()).arg(timeoutMs);
	int pipeDescriptors[2];
	// KWin's buffered QFile on a nonblocking fd can discard its final 16 KiB
	// on flush when a small pipe is full. Unix stream sockets provide more
	// POLLOUT headroom on Linux; this mitigates that writer bug, not all EOFs.
	if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pipeDescriptors) != 0) {
		emit failed(QStringLiteral("Failed to create ScreenShot2 socket pair: %1; %2")
				.arg(QString::fromLocal8Bit(strerror(errno)), context));
		return;
	}

	auto readPipe = QSharedPointer<PipeDescriptor>::create(pipeDescriptors[0]);
	QDBusUnixFileDescriptor writePipe(pipeDescriptors[1]);
	auto message = QDBusMessage::createMethodCall(sKWinService, sKWinPath, sKWinInterface, method);
	auto completeArguments = arguments;
	completeArguments.append(QVariant::fromValue(writePipe));
	message.setArguments(completeArguments);
	QElapsedTimer dispatchTimer;
	dispatchTimer.start();
	auto pendingCall = QDBusConnection::sessionBus().asyncCall(message, timeoutMs);
	::close(pipeDescriptors[1]);

	auto watcher = new QDBusPendingCallWatcher(pendingCall, this);
	connect(watcher, &QDBusPendingCallWatcher::finished, this,
			[this, watcher, readPipe, timeoutMs, context, dispatchTimer] {
				processCaptureReply(watcher, readPipe, timeoutMs, context, dispatchTimer);
			});
}

void KWinScreenShot2Client::processCaptureReply(QDBusPendingCallWatcher *watcher,
														 const QSharedPointer<PipeDescriptor> &readPipe,
														 int readTimeoutMs, const QString &context, const QElapsedTimer &dispatchTimer)
{
	auto replyContext = context + QStringLiteral(" dispatch_to_reply_ms=%1").arg(dispatchTimer.elapsed());
	QDBusPendingReply<QVariantMap> reply = *watcher;
	watcher->deleteLater();
	if (reply.isError()) {
		if (reply.error().name() == sCancellationError) {
			emit canceled();
		} else {
			emit failed(QStringLiteral("%1: %2; %3")
					.arg(reply.error().name(), reply.error().message(), replyContext));
		}
		return;
	}

	readImageAsync(readPipe->release(), reply.value(), readTimeoutMs, replyContext);
}

void KWinScreenShot2Client::readImageAsync(int pipeFd, const QVariantMap &metadata, int timeoutMs, const QString &context)
{
	auto watcher = new QFutureWatcher<ReadResult>(this);
	connect(watcher, &QFutureWatcher<ReadResult>::finished, this, [this, watcher] {
		auto result = watcher->result();
		watcher->deleteLater();
		if (result.image.isNull()) {
			emit failed(result.error);
		} else {
			emit imageReady(result.image);
		}
	});
	QElapsedTimer queueTimer;
	queueTimer.start();
	watcher->setFuture(QtConcurrent::run([pipeFd, metadata, timeoutMs, context, queueTimer] {
		auto queueMs = queueTimer.elapsed();
		QElapsedTimer readTimer;
		readTimer.start();
		auto result = readImage(pipeFd, metadata, timeoutMs);
		if (!result.error.isEmpty()) {
			result.error += QStringLiteral("; %1 worker_queue_ms=%2 worker_read_ms=%3")
					.arg(context).arg(queueMs).arg(readTimer.elapsed());
		}
		return result;
	}));
}

KWinScreenShot2Client::ReadResult KWinScreenShot2Client::readImage(int pipeFd,
																			  const QVariantMap &metadata,
																			  int timeoutMs)
{
	PipeDescriptor pipe(pipeFd);
	if (pipeFd < 0) {
		return errorResult(QLatin1String("Invalid ScreenShot2 pipe descriptor"));
	}
	if (metadata.value(QLatin1String("type")).toString() != QLatin1String("raw")) {
		return errorResult(QLatin1String("ScreenShot2 returned a non-raw image"));
	}

	qint64 widthValue;
	qint64 heightValue;
	qint64 strideValue;
	qint64 formatValue;
	if (!readInteger(metadata.value(QLatin1String("width")), widthValue) ||
		!readInteger(metadata.value(QLatin1String("height")), heightValue) ||
		!readInteger(metadata.value(QLatin1String("stride")), strideValue) ||
		!readInteger(metadata.value(QLatin1String("format")), formatValue)) {
		return errorResult(QLatin1String("ScreenShot2 returned invalid raw image metadata"));
	}
	if (widthValue <= 0 || widthValue > std::numeric_limits<int>::max() ||
		heightValue <= 0 || heightValue > std::numeric_limits<int>::max() ||
		strideValue <= 0 || strideValue > std::numeric_limits<int>::max() ||
		formatValue <= QImage::Format_Invalid || formatValue >= QImage::NImageFormats) {
		return errorResult(QLatin1String("ScreenShot2 raw image metadata is out of range"));
	}

	auto format = static_cast<QImage::Format>(formatValue);
	auto bitsPerPixel = QImage::toPixelFormat(format).bitsPerPixel();
	if (bitsPerPixel <= 0) {
		return errorResult(QLatin1String("ScreenShot2 returned an unsupported image format"));
	}
	auto minimumStride = (static_cast<quint64>(widthValue) * bitsPerPixel + 7) / 8;
	if (static_cast<quint64>(strideValue) < minimumStride) {
		return errorResult(QLatin1String("ScreenShot2 raw image stride is too small"));
	}
	auto byteCount = static_cast<quint64>(strideValue) * static_cast<quint64>(heightValue);
	if (byteCount > static_cast<quint64>(std::numeric_limits<int>::max())) {
		return errorResult(QLatin1String("ScreenShot2 raw image is too large"));
	}

	qreal scale = 1;
	if (metadata.contains(QLatin1String("scale")) &&
		!readScale(metadata.value(QLatin1String("scale")), scale)) {
		return errorResult(QLatin1String("ScreenShot2 returned an invalid image scale"));
	}

	QByteArray content;
	content.resize(static_cast<int>(byteCount));
	qsizetype offset = 0;
	auto transferError = [&](const QString &error) {
		return errorResult(QStringLiteral("%1; expected_bytes=%2 received_bytes=%3 width=%4 height=%5 stride=%6 format=%7 scale=%8")
				.arg(error).arg(byteCount).arg(offset).arg(widthValue).arg(heightValue)
				.arg(strideValue).arg(formatValue).arg(scale, 0, 'g', 16));
	};
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while (offset < content.size()) {
		auto now = std::chrono::steady_clock::now();
		if (now >= deadline) {
			return transferError(QLatin1String("Timed out while reading ScreenShot2 image data"));
		}
		auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
		pollfd pollDescriptor { pipeFd, POLLIN, 0 };
		int pollResult;
		do {
			pollResult = ::poll(&pollDescriptor, 1, static_cast<int>(std::max<qint64>(1, remainingMs)));
		} while (pollResult < 0 && errno == EINTR);
		if (pollResult == 0) {
			return transferError(QLatin1String("Timed out while reading ScreenShot2 image data"));
		}
		if (pollResult < 0 || (pollDescriptor.revents & (POLLERR | POLLNVAL))) {
			return transferError(QStringLiteral("Failed to wait for ScreenShot2 image data: %1")
									.arg(QString::fromLocal8Bit(strerror(errno))));
		}
		if (!(pollDescriptor.revents & (POLLIN | POLLHUP))) {
			continue;
		}

		ssize_t bytesRead;
		do {
			bytesRead = ::read(pipeFd,
								 content.data() + offset,
								 static_cast<size_t>(content.size() - offset));
		} while (bytesRead < 0 && errno == EINTR);
		if (bytesRead < 0) {
			return transferError(QStringLiteral("Failed to read ScreenShot2 image data: %1")
									.arg(QString::fromLocal8Bit(strerror(errno))));
		}
		if (bytesRead == 0) {
			return transferError(QLatin1String("ScreenShot2 image data ended before the image was complete"));
		}
		offset += bytesRead;
	}

	QImage rawImage(reinterpret_cast<const uchar *>(content.constData()),
							static_cast<int>(widthValue),
							static_cast<int>(heightValue),
							static_cast<int>(strideValue),
							format);
	auto image = rawImage.copy();
	if (image.isNull()) {
		return transferError(QLatin1String("Failed to construct the ScreenShot2 image"));
	}
	image.setDevicePixelRatio(scale);
	return { image, QString() };
}

QVariantMap KWinScreenShot2Client::createOptions(bool captureCursor)
{
	return { { QLatin1String("include-cursor"), captureCursor } };
}
