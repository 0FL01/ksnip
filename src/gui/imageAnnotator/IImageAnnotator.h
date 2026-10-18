/*
 * Copyright (C) 2020 Damir Porobic <damir.porobic@gmx.com>
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

#ifndef KSNIP_IIMAGEANNOTATOR_H
#define KSNIP_IIMAGEANNOTATOR_H

#include <QObject>
#include <QImage>
#include <QList>
#include <QTimer>
#include <functional>

class QAction;

class IImageAnnotator : public QObject
{
Q_OBJECT
public:
	explicit IImageAnnotator() = default;
	~IImageAnnotator() override = default;
	virtual QImage image() const = 0;
	virtual QImage imageAt(int index) const = 0;
	virtual QAction *undoAction() = 0;
	virtual QAction *redoAction() = 0;
	virtual QSize sizeHint() const = 0;
	virtual void showAnnotator() = 0;
	virtual void showCropper() = 0;
	virtual void showScaler() = 0;
	virtual void showCanvasModifier() = 0;
	virtual void showRotator() = 0;
	virtual void showCutter() = 0;
	virtual void setSettingsCollapsed(bool isCollapsed) = 0;
	virtual void hide() = 0;
	virtual void close() = 0;
	virtual bool isVisible() const = 0;
	virtual QWidget* widget() const = 0;
	// Optional backing support. Older annotators retain the synchronous API.
	virtual bool enableRasterBacking(const QString &directory = QString()) { Q_UNUSED(directory) return false; }
	virtual bool rasterBackingEnabled() const { return false; }
	virtual bool isCurrentImageReady() const { return true; }
	virtual bool canAcceptImage() const { return true; }
	virtual bool reserveImageInput() { return canAcceptImage(); }
	virtual void releaseImageInput() {}
	virtual void closeRasterBacking(const std::function<void()> &finished)
	{
		QTimer::singleShot(0, this, [finished]() { if (finished) { finished(); } });
	}
	virtual void requestImageAt(int index, quint64 requestId)
	{
		const auto result = imageAt(index);
		QTimer::singleShot(0, this, [this, requestId, result] {
			emit imageRequestFinished(requestId, result, false,
				result.isNull() ? tr("The image is not available.") : QString());
		});
	}
	virtual void cancelImageRequest(quint64 requestId) { Q_UNUSED(requestId) }
	virtual void beginTabBatch() {}
	virtual void endTabBatch() {}

public slots:
	virtual void loadImage(const QPixmap &pixmap) = 0;
	virtual int addTab(const QPixmap &pixmap, const QString &title, const QString &toolTip) = 0;
	virtual void updateTabInfo(int index, const QString &title, const QString &toolTip) = 0;
	virtual void insertImageItem(const QPointF &position, const QPixmap &pixmap) = 0;
	virtual void setSmoothPathEnabled(bool enabled) = 0;
	virtual void setSaveToolSelection(bool enabled) = 0;
	virtual void setSmoothFactor(int factor) = 0;
	virtual void setSwitchToSelectToolAfterDrawingItem(bool enabled) = 0;
	virtual void setSelectItemAfterDrawing(bool enabled) = 0;
	virtual void setNumberToolSeedChangeUpdatesAllItems(bool enabled) = 0;
	virtual void setTabBarAutoHide(bool enabled) = 0;
	virtual void removeTab(int index) = 0;
	virtual void setStickers(const QStringList &stickerPaths, bool keepDefault) = 0;
	virtual void addTabContextMenuActions(const QList<QAction*> & actions) = 0;
	virtual void setCanvasColor(const QColor &color) = 0;
	virtual void setControlsWidgetVisible(bool isVisible) = 0;

signals:
	void imageChanged() const;
	void currentTabChanged(int index) const;
	void tabCloseRequested(int index) const;
	void tabMoved(int fromIndex, int toIndex);
	void tabContextMenuOpened(int index) const;
	void currentImageReadyChanged(bool ready);
	void imageAdmissionChanged(bool allowed);
	void imageRequestFinished(quint64 requestId, const QImage &image, bool canceled, const QString &error);
	void rasterBackingError(const QString &message);
	void tabCloseBatchRequested(const QList<int> &indices);
};


#endif //KSNIP_IIMAGEANNOTATOR_H
