/*
 * Copyright (C) 2026 kImageAnnotator contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "RasterStore.h"

namespace kImageAnnotator {

RasterDescriptor::RasterDescriptor(const QImage &image) :
	size(image.size()), format(image.format()), devicePixelRatio(image.devicePixelRatio()),
	palette(image.colorTable()), colorSpace(image.colorSpace()),
	dotsPerMeterX(image.dotsPerMeterX()), dotsPerMeterY(image.dotsPerMeterY()), offset(image.offset())
{
	for (const auto &key : image.textKeys()) {
		text.insert(key, image.text(key));
	}
	rowBytes = (qint64(image.width()) * image.depth() + 7) / 8;
	byteCount = rowBytes * image.height();
}

bool RasterDescriptor::isValid() const
{
	if (size.width() <= 0 || size.height() <= 0 || format <= QImage::Format_Invalid ||
		format >= QImage::NImageFormats || !qIsFinite(devicePixelRatio) || devicePixelRatio <= 0 ||
		rowBytes <= 0 || rowBytes > std::numeric_limits<int>::max() ||
		byteCount <= 0 || byteCount != rowBytes * size.height()) {
		return false;
	}
	// A one-pixel probe validates the format without allocating the screenshot.
	QImage probe(1, 1, format);
	return !probe.isNull() && rowBytes == (qint64(size.width()) * probe.depth() + 7) / 8 &&
		byteCount <= std::numeric_limits<qsizetype>::max();
}

QByteArray RasterDescriptor::canonical() const
{
	QByteArray bytes;
	QDataStream stream(&bytes, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_5_15);
	stream.setByteOrder(QDataStream::BigEndian);
	stream << qint32(format) << size << double(devicePixelRatio) << palette
		   << colorSpace << qint32(dotsPerMeterX) << qint32(dotsPerMeterY) << offset << text;
	return bytes;
}

QImage RasterDescriptor::allocate() const
{
	if (!isValid()) {
		return {};
	}
	QImage image(size, format);
	if (image.isNull() || image.bytesPerLine() < rowBytes) {
		return {};
	}
	image.setDevicePixelRatio(devicePixelRatio);
	image.setColorTable(palette);
	image.setColorSpace(colorSpace);
	image.setDotsPerMeterX(dotsPerMeterX);
	image.setDotsPerMeterY(dotsPerMeterY);
	image.setOffset(offset);
	for (auto it = text.cbegin(); it != text.cend(); ++it) {
		image.setText(it.key(), it.value());
	}
	return image;
}

class RasterWorker : public QObject
{
	Q_OBJECT
public:
	QString directory;
	QHash<QByteArray, QList<QWeakPointer<RasterBlob>>> candidates;

	// Borrow ordinary rows; copy only one significant row when ignored bits must
	// be normalized. Never change the source image or convert its pixel format.
	static const char *canonicalRow(const RasterDescriptor &descriptor, const uchar *source, QByteArray &scratch)
	{
		auto writableRow = [&]() {
			scratch.resize(int(descriptor.rowBytes));
			memcpy(scratch.data(), source, size_t(descriptor.rowBytes));
			return reinterpret_cast<uchar *>(scratch.data());
		};
		constexpr int highByte16 = Q_BYTE_ORDER == Q_LITTLE_ENDIAN ? 1 : 0;
		constexpr int highByte32 = Q_BYTE_ORDER == Q_LITTLE_ENDIAN ? 3 : 0;
		int stride = 0;
		int offset = 0;
		int componentBytes = 1;
		uchar mask = 0;
		uchar opaque = 0;
		quint16 opaque16 = 0xffff;
		switch (descriptor.format) {
		case QImage::Format_Mono:
		case QImage::Format_MonoLSB: {
			auto bits = descriptor.size.width() % 8;
			if (!bits) { return reinterpret_cast<const char *>(source); }
			auto row = writableRow();
			row[descriptor.rowBytes - 1] &= descriptor.format == QImage::Format_Mono ?
				uchar(0xff << (8 - bits)) : uchar((1 << bits) - 1);
			return scratch.constData();
		}
		case QImage::Format_RGB32:
			stride = 4; offset = highByte32; opaque = 0xff;
			break;
		case QImage::Format_RGBX8888:
			stride = 4; offset = 3; opaque = 0xff;
			break;
		case QImage::Format_RGBX64:
			stride = 8; offset = 6; componentBytes = 2;
			break;
		case QImage::Format_RGB30:
		case QImage::Format_BGR30:
			stride = 4; offset = highByte32; mask = 0x3f; opaque = 0xc0;
			break;
		case QImage::Format_RGB555:
			stride = 2; offset = highByte16; mask = 0x7f;
			break;
		case QImage::Format_RGB444:
			stride = 2; offset = highByte16; mask = 0x0f;
			break;
		case QImage::Format_RGB666:
			stride = 3; mask = 0x03; // Qt's packed 24-bit pixels are byte-ordered MSB first.
			break;
		case QImage::Format_ARGB8555_Premultiplied:
			stride = 3; mask = 0x7f; // The high padding bit is not part of its 8-bit alpha.
			break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
		case QImage::Format_RGBX16FPx4:
			stride = 8; offset = 6; componentBytes = 2; opaque16 = 0x3c00; // Binary16 1.0.
			break;
		case QImage::Format_RGBX32FPx4:
			stride = 16; offset = 12; componentBytes = 4;
			break;
#endif
		default:
			return reinterpret_cast<const char *>(source);
		}
		auto row = writableRow();
		const quint32 opaque32 = 0x3f800000; // Binary32 1.0, in native component byte order.
		for (qint64 byte = offset; byte < descriptor.rowBytes; byte += stride) {
			if (componentBytes == 2) {
				memcpy(row + byte, &opaque16, sizeof(opaque16));
			} else if (componentBytes == 4) {
				memcpy(row + byte, &opaque32, sizeof(opaque32));
			} else {
				row[byte] = (row[byte] & mask) | opaque;
			}
		}
		return scratch.constData();
	}

	static QByteArray digest(const RasterDescriptor &descriptor, const QImage &image)
	{
		QCryptographicHash hash(QCryptographicHash::Sha256);
		hash.addData(descriptor.canonical());
		QByteArray scratch;
		for (int row = 0; row < image.height(); ++row) {
			hash.addData(canonicalRow(descriptor, image.constScanLine(row), scratch), int(descriptor.rowBytes));
		}
		return hash.result();
	}

	static bool equals(const RasterBlob &blob, const RasterDescriptor &descriptor, const QImage &image)
	{
		if (blob.metadata != descriptor.canonical() || blob.colorSpace != descriptor.colorSpace ||
			blob.byteCount != descriptor.byteCount) {
			return false;
		}
		QFile file(blob.path);
		if (!file.open(QIODevice::ReadOnly) || file.size() != descriptor.byteCount) {
			return false;
		}
		// Worker-written files already contain canonical rows; compare the new
		// source's canonical bytes through the existing bounded I/O buffer.
		QByteArray buffer;
		buffer.resize(int(qMin<qint64>(descriptor.rowBytes, 64 * 1024)));
		QByteArray scratch;
		for (int row = 0; row < image.height(); ++row) {
			auto canonical = canonicalRow(descriptor, image.constScanLine(row), scratch);
			qint64 offset = 0;
			while (offset < descriptor.rowBytes) {
				auto count = qMin<qint64>(buffer.size(), descriptor.rowBytes - offset);
				auto read = file.read(buffer.data(), count);
				if (read <= 0) {
					return false;
				}
				if (memcmp(buffer.constData(), canonical + offset, size_t(read)) != 0) {
					return false;
				}
				offset += read;
			}
		}
		return true;
	}

signals:
	void operationFinished();
};

class RasterSession
{
public:
	QThread *thread = nullptr;
	RasterWorker *worker = nullptr;
	QString directory;

	~RasterSession()
	{
		// No GUI wait and no GUI recursive removal. This is queued after all jobs
		// and all last-blob deletions because every blob retains this session.
		auto path = directory;
		auto target = worker;
		QMetaObject::invokeMethod(target, [path, target]() {
			QDir dir(path);
			for (const auto &file : dir.entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot)) {
				if (!dir.remove(file)) { qWarning("Cannot remove a screenshot backing file during session cleanup."); }
			}
			if (dir.exists() && !QDir().rmdir(path)) { qWarning("Cannot remove the screenshot backing session directory."); }
			target->thread()->quit();
		}, Qt::QueuedConnection);
	}
};

RasterBlob::~RasterBlob()
{
	if (session) {
		auto file = path;
		QMetaObject::invokeMethod(session->worker, [file]() {
			if (QFile::exists(file) && !QFile::remove(file)) { qWarning("Cannot remove an unreferenced screenshot backing file."); }
		}, Qt::QueuedConnection);
	}
}

RasterDocument::RasterDocument(QObject *parent) : QObject(parent)
{}

RasterDocument::~RasterDocument()
{
	setResident(false);
}

void RasterDocument::attach(const RasterHandle &handle)
{
	if (!handle) {
		return;
	}
	auto &entry = mEntries[handle.data()];
	if (entry.users++ == 0) {
		entry.handle = handle;
		if (mResident) {
			++handle->leases;
		}
		emit resourcesChanged();
	}
}

void RasterDocument::detach(const RasterHandle &handle)
{
	if (!handle) {
		return;
	}
	auto it = mEntries.find(handle.data());
	if (it != mEntries.end() && --it->users == 0) {
		if (mResident) {
			--handle->leases;
		}
		mEntries.erase(it);
		releaseUnleased({handle});
		emit resourcesChanged();
	}
}

RasterHandles RasterDocument::handles() const
{
	RasterHandles result;
	for (const auto &entry : mEntries) {
		auto handle = entry.handle.toStrongRef();
		if (handle) {
			result.append(handle);
		}
	}
	return result;
}

void RasterDocument::setResident(bool resident)
{
	if (mResident == resident) {
		return;
	}
	mResident = resident;
	auto resources = handles();
	for (const auto &handle : resources) {
		handle->leases += resident ? 1 : -1;
	}
	if (!resident) {
		releaseUnleased(resources);
	}
}

bool RasterDocument::isResident() const { return mResident; }

bool RasterDocument::isDecoded() const
{
	for (const auto &handle : handles()) {
		if (handle->image.isNull()) {
			return false;
		}
	}
	return true;
}

bool RasterDocument::needsBacking() const
{
	for (const auto &handle : handles()) {
		if (!handle->blob) {
			return true;
		}
	}
	return false;
}

void RasterDocument::releaseUnleased(const RasterHandles &handles)
{
	for (const auto &handle : handles) {
		if (handle->leases == 0 && handle->blob) {
			handle->image = QImage();
		}
	}
}

RasterImage::RasterImage(const QImage &image, RasterDocument *document)
{
	if (!image.isNull()) {
		mHandle.reset(new RasterData{RasterDescriptor(image), image, {}, 0});
	}
	bind(document);
}

RasterImage::RasterImage(const QPixmap &image, RasterDocument *document) : RasterImage(image.toImage(), document)
{}

RasterImage::RasterImage(const RasterImage &other) : mHandle(other.mHandle)
{
	bind(other.mDocument);
}

RasterImage &RasterImage::operator=(const RasterImage &other)
{
	if (this != &other) {
		replace(other.mHandle);
	}
	return *this;
}

RasterImage &RasterImage::operator=(const QImage &image)
{
	RasterHandle handle;
	if (!image.isNull()) {
		handle.reset(new RasterData{RasterDescriptor(image), image, {}, 0});
	}
	replace(handle);
	return *this;
}

RasterImage &RasterImage::operator=(const QPixmap &image) { return operator=(image.toImage()); }
RasterImage::~RasterImage() { bind(nullptr); }

void RasterImage::bind(RasterDocument *document)
{
	if (document == mDocument) {
		return;
	}
	// Transfer the lease before detaching the old owner. A sole backed handle
	// moved between two resident documents must not lose its decoded bytes.
	if (document) { document->attach(mHandle); }
	if (mDocument) { mDocument->detach(mHandle); }
	mDocument = document;
}

void RasterImage::replace(const RasterHandle &handle)
{
	if (handle == mHandle) {
		return;
	}
	// Attach first: replacing one slot must not momentarily evict shared data.
	if (mDocument) {
		mDocument->attach(handle);
		mDocument->detach(mHandle);
	}
	mHandle = handle;
}

const QImage &RasterImage::image() const
{
	static const QImage empty;
	return mHandle ? mHandle->image : empty;
}

QPixmap RasterImage::pixmap() const { return QPixmap::fromImage(image()); }
bool RasterImage::isNull() const { return !mHandle; }
RasterHandle RasterImage::handle() const { return mHandle; }

RasterImage RasterImage::fromItem(QGraphicsPixmapItem *item, RasterDocument *document)
{
	if (auto backed = dynamic_cast<RasterPixmapItem *>(item)) {
		auto raster = backed->raster();
		raster.bind(document);
		return raster;
	}
	return RasterImage(item->pixmap(), document);
}

void RasterImage::apply(QGraphicsPixmapItem *item) const
{
	if (auto backed = dynamic_cast<RasterPixmapItem *>(item)) {
		backed->setRaster(*this);
	} else {
		item->setPixmap(pixmap());
	}
}

RasterPixmapItem::RasterPixmapItem(const QPixmap &pixmap, RasterDocument *document) :
	QGraphicsPixmapItem(pixmap), mRaster(pixmap, document), mRect(QGraphicsPixmapItem::boundingRect())
{}

RasterImage RasterPixmapItem::raster() const { return mRaster; }

void RasterPixmapItem::setRaster(const RasterImage &raster)
{
	prepareGeometryChange();
	mRaster = raster;
	QGraphicsPixmapItem::setPixmap(mRaster.pixmap());
	mRect = QGraphicsPixmapItem::boundingRect();
}

void RasterPixmapItem::refreshDecodedPixmap()
{
	// Clearing a backing pixmap does not change document geometry.
	QGraphicsPixmapItem::setPixmap(mRaster.pixmap());
}

QRectF RasterPixmapItem::boundingRect() const { return mRect; }
QPainterPath RasterPixmapItem::shape() const
{
	if (!pixmap().isNull()) { return QGraphicsPixmapItem::shape(); }
	QPainterPath path;
	path.addRect(mRect);
	return path;
}

RasterStore::RasterStore(QObject *parent) : QObject(parent) {}
RasterStore::~RasterStore() = default;

bool RasterStore::validateStorage(const QString &directory, QString *error)
{
	// QStorageInfo needs an existing path. Walk only the finite parent chain;
	// resolve symlinks before querying the filesystem that will own the files.
	QFileInfo existing(QDir::cleanPath(QFileInfo(directory).absoluteFilePath()));
	while (!existing.exists()) {
		auto parent = existing.absolutePath();
		if (parent == existing.absoluteFilePath()) {
			*error = tr("Cannot find an existing parent for screenshot cache directory %1.").arg(directory);
			return false;
		}
		existing.setFile(parent);
	}
	if (!existing.isDir()) {
		*error = tr("The screenshot cache path %1 has a parent that is not a directory.").arg(directory);
		return false;
	}
	QStorageInfo storage(existing.canonicalFilePath());
	storage.refresh();
	auto type = storage.fileSystemType().trimmed().toLower();
	auto device = storage.device();
	if (type == "tmpfs" || type == "ramfs" || type == "rootfs" || type == "devtmpfs" || type == "hugetlbfs" || type == "mfs" ||
		device.startsWith("/dev/ram") || device.startsWith("/dev/zram")) {
		*error = tr("Screenshot cache directory %1 is on a memory-backed filesystem (%2, %3). Choose a disk-backed directory.")
			.arg(directory, QString::fromLatin1(type), QString::fromLocal8Bit(device));
		return false;
	}
	if (!storage.isValid() || !storage.isReady() || type.isEmpty()) {
		*error = tr("Cannot verify a disk-backed filesystem for screenshot cache directory %1.").arg(directory);
		return false;
	}
	if (storage.isReadOnly()) {
		*error = tr("The filesystem for screenshot cache directory %1 is read-only.").arg(directory);
		return false;
	}
	error->clear();
	return true;
}

bool RasterStore::initialize(const QString &directory, QString *error)
{
	if (mSession) {
		error->clear();
		return true;
	}
	// Only storage metadata and directory creation are synchronous, before any
	// document exists. Raster I/O, comparisons, hashes and cleanup use the worker.
	QStringList candidates;
	if (!directory.isEmpty()) {
		candidates.append(directory); // Never silently redirect an explicit path.
	} else {
		auto cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
		auto data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
		if (!cache.isEmpty()) { candidates.append(QDir(cache).filePath(QStringLiteral("captures"))); }
		if (!data.isEmpty()) { candidates.append(QDir(data).filePath(QStringLiteral("cache/captures"))); }
		candidates.removeDuplicates(); // At most two user-local choices; no temp-dir or RAM fallback.
	}
	QString failure = tr("No user-local disk-backed cache location is available.");
	for (const auto &candidate : qAsConst(candidates)) {
		auto base = QDir::cleanPath(QFileInfo(candidate).absoluteFilePath());
		if (!validateStorage(base, &failure)) { continue; }
		if (!QDir().mkpath(base)) {
			failure = tr("Cannot create screenshot cache directory %1.").arg(base);
			continue;
		}
		QTemporaryDir temporary(QDir(base).filePath(QStringLiteral("session-XXXXXX")));
		temporary.setAutoRemove(false); // Never let its destructor recurse on the GUI.
		if (!temporary.isValid()) {
			failure = tr("Cannot create a private screenshot cache session in %1.").arg(base);
			continue;
		}
		// Verify the actual session as well, in case a symlink/mount changed while
		// its missing parent directories were being created.
		if (!validateStorage(temporary.path(), &failure)) {
			QDir().rmdir(temporary.path());
			continue;
		}
		if (!QFile::setPermissions(temporary.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
			failure = tr("Cannot set private screenshot cache permissions in %1.").arg(base);
			QDir().rmdir(temporary.path());
			continue;
		}
		mSession.reset(new RasterSession);
		mSession->directory = temporary.path();
		mSession->thread = new QThread;
		mSession->worker = new RasterWorker;
		mSession->worker->directory = temporary.path();
		mSession->worker->moveToThread(mSession->thread);
		connect(mSession->thread, &QThread::finished, mSession->worker, &QObject::deleteLater);
		connect(mSession->thread, &QThread::finished, mSession->thread, &QObject::deleteLater);
		connect(mSession->worker, &RasterWorker::operationFinished, this, [this]() {
			auto delivery = std::move(mDeliverResult);
			mBusy = false;
			if (delivery) { delivery(); }
		}, Qt::QueuedConnection);
		mSession->thread->start();
		error->clear();
		return true;
	}
	*error = tr("Cannot initialize a disk-backed screenshot cache: %1").arg(failure);
	return false;
}

bool RasterStore::isBusy() const { return mBusy; }
QString RasterStore::sessionDirectory() const { return mSession ? mSession->directory : QString(); }

void RasterStore::close(const std::function<void()> &finished)
{
	if (!mSession) {
		QMetaObject::invokeMethod(this, [finished]() { if (finished) { finished(); } }, Qt::QueuedConnection);
		return;
	}
	// Consumers have been released. In-flight jobs still own their session and
	// snapshots; its final release queues cleanup, then stops the worker thread.
	connect(mSession->thread, &QThread::finished, this, [finished]() {
		if (finished) { finished(); }
	}, Qt::QueuedConnection);
	mDeliverResult = {};
	mSession.clear();
}

void RasterStore::prepare(const RasterHandles &handles, bool decode,
						  const std::function<void(const QString &)> &finished)
{
	Q_ASSERT(mSession && !mBusy);
	mBusy = true;
	struct Work {
		RasterHandle handle;
		QImage source;
		QSharedPointer<RasterBlob> blob;
		QImage decoded;
	};
	struct Operation {
		QList<Work> work;
		QString error;
	};
	auto operation = QSharedPointer<Operation>::create();
	QSet<RasterData *> seen;
	for (const auto &handle : handles) {
		if (handle && !seen.contains(handle.data())) {
			seen.insert(handle.data());
			operation->work.append({handle, handle->image, handle->blob, {}});
		}
	}
	auto session = mSession;
	// The worker never invokes a GUI pointer, even a QPointer: testing it and
	// posting to it would race destruction. Qt's queued signal connection owns
	// delivery and is automatically disconnected when this store disappears.
	mDeliverResult = [operation, finished]() {
		for (auto &entry : operation->work) {
			if (entry.blob) { entry.handle->blob = entry.blob; }
			if (operation->error.isEmpty() && !entry.decoded.isNull()) {
				entry.handle->image = entry.decoded;
			}
		}
		operation->work.clear(); // Drop worker source copies before any eviction.
		finished(operation->error);
	};
	QMetaObject::invokeMethod(session->worker, [session, operation, decode]() {
		auto &error = operation->error;
		for (auto &entry : operation->work) {
			const auto &descriptor = entry.handle->descriptor;
			if (!descriptor.isValid()) {
				error = tr("Invalid screenshot raster descriptor.");
				break;
			}
			if (!entry.source.isNull() && (RasterDescriptor(entry.source).canonical() != descriptor.canonical() ||
				entry.source.size() != descriptor.size || entry.source.bytesPerLine() < descriptor.rowBytes)) {
				error = tr("The decoded screenshot does not match its raster descriptor.");
				break;
			}
			if (!entry.blob) {
				if (entry.source.isNull()) {
					error = tr("The last decoded screenshot copy is missing.");
					break;
				}
				auto digest = RasterWorker::digest(descriptor, entry.source);
				auto &candidates = session->worker->candidates[digest];
				for (auto it = candidates.begin(); it != candidates.end();) {
					auto candidate = it->toStrongRef();
					if (!candidate) {
						it = candidates.erase(it);
					} else {
						if (RasterWorker::equals(*candidate, descriptor, entry.source)) {
							entry.blob = candidate;
							break;
						}
						++it;
					}
				}
				if (!entry.blob) {
					auto path = QDir(session->directory).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
					QSaveFile file(path);
					file.setDirectWriteFallback(false);
					if (!file.open(QIODevice::WriteOnly) ||
						!file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) {
						error = tr("Cannot open a private screenshot cache file: %1").arg(file.errorString());
						break;
					}
					QByteArray scratch;
					for (int row = 0; row < entry.source.height() && error.isEmpty(); ++row) {
						auto canonical = RasterWorker::canonicalRow(descriptor, entry.source.constScanLine(row), scratch);
						qint64 written = 0;
						while (written < descriptor.rowBytes) {
							auto count = file.write(canonical + written, descriptor.rowBytes - written);
							if (count <= 0) {
								error = tr("Cannot write the screenshot cache: %1").arg(file.errorString());
								break;
							}
							written += count;
						}
					}
					if (!error.isEmpty()) {
						file.cancelWriting();
						break;
					}
					if (!file.commit()) {
						error = tr("Cannot commit the screenshot cache: %1").arg(file.errorString());
						break;
					}
					entry.blob.reset(new RasterBlob{path, descriptor.canonical(), digest, descriptor.byteCount, session, descriptor.colorSpace});
					candidates.append(entry.blob);
				}
			}
			if (decode && entry.source.isNull()) {
				QFile file(entry.blob->path);
				if (entry.blob->metadata != descriptor.canonical() || entry.blob->colorSpace != descriptor.colorSpace ||
					entry.blob->byteCount != descriptor.byteCount ||
					!file.open(QIODevice::ReadOnly) || file.size() != descriptor.byteCount) {
					error = tr("The screenshot cache file is missing or has an invalid length.");
					break;
				}
				entry.decoded = descriptor.allocate();
				if (entry.decoded.isNull()) {
					error = tr("Cannot allocate the screenshot raster.");
					break;
				}
				QCryptographicHash hash(QCryptographicHash::Sha256);
				hash.addData(descriptor.canonical());
				QByteArray scratch;
				for (int row = 0; row < descriptor.size.height() && error.isEmpty(); ++row) {
					auto decodedRow = entry.decoded.scanLine(row);
					qint64 offset = 0;
					while (offset < descriptor.rowBytes) {
						auto count = file.read(reinterpret_cast<char *>(decodedRow) + offset, descriptor.rowBytes - offset);
						if (count <= 0) {
							error = tr("Unexpected end of the screenshot cache file.");
							break;
						}
						offset += count;
					}
					if (error.isEmpty()) {
						auto canonical = RasterWorker::canonicalRow(descriptor, decodedRow, scratch);
						hash.addData(canonical, int(descriptor.rowBytes));
						if (canonical != reinterpret_cast<const char *>(decodedRow)) {
							memcpy(decodedRow, canonical, size_t(descriptor.rowBytes));
						}
					}
				}
				if (error.isEmpty() && (file.pos() != descriptor.byteCount || file.size() != descriptor.byteCount ||
					file.error() != QFileDevice::NoError)) {
					error = tr("The screenshot cache file has an invalid length or read error.");
				}
				if (error.isEmpty() && hash.result() != entry.blob->digest) {
					error = tr("The screenshot cache digest does not match.");
				}
				if (!error.isEmpty()) {
					entry.decoded = QImage();
					break;
				}
			}
		}
		emit session->worker->operationFinished();
	}, Qt::QueuedConnection);
}

} // namespace kImageAnnotator

#include "RasterStore.moc"
