/*
 * Copyright (C) 2026 kImageAnnotator contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef KIMAGEANNOTATOR_RASTERSTORE_H
#define KIMAGEANNOTATOR_RASTERSTORE_H

#include <QColorSpace>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QHash>
#include <QImage>
#include <QMap>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>
#include <QVector>
#include <QtMath>
#include <cstring>
#include <functional>
#include <limits>
#include <utility>

namespace kImageAnnotator {

// The descriptor, not a file header, is the authority for allocation and layout.
struct RasterDescriptor
{
	QSize size;
	QImage::Format format = QImage::Format_Invalid;
	qreal devicePixelRatio = 1;
	QVector<QRgb> palette;
	QColorSpace colorSpace;
	int dotsPerMeterX = 0;
	int dotsPerMeterY = 0;
	QPoint offset;
	QMap<QString, QString> text;
	qint64 rowBytes = 0;
	qint64 byteCount = 0;

	explicit RasterDescriptor(const QImage &image = QImage());
	bool isValid() const;
	QByteArray canonical() const;
	QImage allocate() const;
};

class RasterSession;
struct RasterBlob
{
	QString path;
	QByteArray metadata;
	QByteArray digest;
	qint64 byteCount = 0;
	QSharedPointer<RasterSession> session;
	QColorSpace colorSpace;
	~RasterBlob();
};

struct RasterData
{
	RasterDescriptor descriptor;
	QImage image;
	QSharedPointer<RasterBlob> blob;
	int leases = 0;
};
using RasterHandle = QSharedPointer<RasterData>;
using RasterHandles = QList<RasterHandle>;

class RasterDocument : public QObject
{
	Q_OBJECT
public:
	explicit RasterDocument(QObject *parent = nullptr);
	~RasterDocument() override;
	void attach(const RasterHandle &handle);
	void detach(const RasterHandle &handle);
	RasterHandles handles() const;
	void setResident(bool resident);
	bool isResident() const;
	bool isDecoded() const;
	bool needsBacking() const;
	static void releaseUnleased(const RasterHandles &handles);

signals:
	void resourcesChanged();

private:
	struct Entry {
		QWeakPointer<RasterData> handle;
		int users = 0;
	};
	QHash<RasterData *, Entry> mEntries;
	bool mResident = true;
};

// Each slot registers its current handle. Replacing a raster unregisters the old
// handle immediately; truncated redo branches therefore cannot pin old blobs.
class RasterImage
{
public:
	RasterImage() = default;
	explicit RasterImage(const QImage &image, RasterDocument *document = nullptr);
	explicit RasterImage(const QPixmap &image, RasterDocument *document = nullptr);
	RasterImage(const RasterImage &other);
	RasterImage &operator=(const RasterImage &other);
	RasterImage &operator=(const QImage &image);
	RasterImage &operator=(const QPixmap &image);
	~RasterImage();
	void bind(RasterDocument *document);
	const QImage &image() const;
	QPixmap pixmap() const;
	bool isNull() const;
	RasterHandle handle() const;
	static RasterImage fromItem(QGraphicsPixmapItem *item, RasterDocument *document = nullptr);
	void apply(QGraphicsPixmapItem *item) const;

private:
	RasterHandle mHandle;
	QPointer<RasterDocument> mDocument;
	void replace(const RasterHandle &handle);
};

class RasterPixmapItem : public QGraphicsPixmapItem
{
public:
	RasterPixmapItem(const QPixmap &pixmap, RasterDocument *document);
	RasterImage raster() const;
	void setRaster(const RasterImage &raster);
	void refreshDecodedPixmap();
	QRectF boundingRect() const override;
	QPainterPath shape() const override;

private:
	RasterImage mRaster;
	QRectF mRect;
};

// Only one operation may be in flight. Callers queue document IDs, never frames.
class RasterStore : public QObject
{
	Q_OBJECT
public:
	explicit RasterStore(QObject *parent = nullptr);
	~RasterStore() override;
	bool initialize(const QString &directory, QString *error);
	bool isBusy() const;
	QString sessionDirectory() const;
	void close(const std::function<void()> &finished);
	void prepare(const RasterHandles &handles, bool decode,
			 const std::function<void(const QString &)> &finished);

private:
	QSharedPointer<RasterSession> mSession;
	bool mBusy = false;
	std::function<void()> mDeliverResult;
	static bool validateStorage(const QString &directory, QString *error);
};

} // namespace kImageAnnotator

#endif
