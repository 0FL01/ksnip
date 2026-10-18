/*
 * Copyright (C) 2018 Damir Porobic <damir.porobic@gmx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef KIMAGEANNOTATOR_KIMAGEANNOTATOR_H
#define KIMAGEANNOTATOR_KIMAGEANNOTATOR_H

#include <QWidget>
#include <QImage>
#include <QList>

#include <kImageAnnotator/KImageAnnotatorExport.h>

#define KIMAGEANNOTATOR_HAS_RASTER_BACKING 1

namespace kImageAnnotator {

KIMAGEANNOTATOR_EXPORT void loadTranslations();

class KImageAnnotatorPrivate;

class KIMAGEANNOTATOR_EXPORT KImageAnnotator : public QWidget
{
Q_OBJECT

	Q_DECLARE_PRIVATE(KImageAnnotator)

public:
	explicit KImageAnnotator();
	~KImageAnnotator() override;
	QImage image() const;
	QImage imageAt(int index) const;
	// Opt-in, before opening documents. The default retains legacy semantics.
	bool enableRasterBacking(const QString &directory = QString());
	bool rasterBackingEnabled() const;
	bool isCurrentImageReady() const;
	bool canAcceptImage() const;
	// Reserve before acquiring pixels. One admitted image remains acceptable if
	// persistence fails while an asynchronous capture is in flight.
	bool reserveImageInput();
	void releaseImageInput();
	void closeRasterBacking();
	// Resolve the tab now; completion (also a cache hit/error/cancel) is queued.
	// Request IDs must be unique until their terminal signal is delivered.
	void requestImageAt(int index, quint64 requestId);
	void cancelImageRequest(quint64 requestId);
	void beginTabBatch();
	void endTabBatch();
	QAction *undoAction();
	QAction *redoAction();
	QSize sizeHint() const override;
	void showAnnotator();
	void showCropper();
	void showScaler();
	void showRotator();
	void showCanvasModifier();
	void showCutter();

public Q_SLOTS:
	void loadImage(const QPixmap &pixmap);
	int addTab(const QPixmap &pixmap, const QString &title, const QString &toolTip);
	void updateTabInfo(int index, const QString &title, const QString &toolTip);
	void insertImageItem(const QPointF &position, const QPixmap &pixmap);
	Q_DECL_DEPRECATED void setTextFont(const QFont &font);
	Q_DECL_DEPRECATED void setNumberFont(const QFont &font);
	Q_DECL_DEPRECATED void setItemShadowEnabled(bool enabled);
	void setSmoothPathEnabled(bool enabled);
	void setSaveToolSelection(bool enabled);
	void setSmoothFactor(int factor);
	void setSwitchToSelectToolAfterDrawingItem(bool enabled);
	void setNumberToolSeedChangeUpdatesAllItems(bool enabled);
	void setTabBarAutoHide(bool enabled);
	void removeTab(int index);
	void setStickers(const QStringList &stickerPaths, bool keepDefault);
	void addTabContextMenuActions(const QList<QAction*> & actions);
	void setSettingsCollapsed(bool isCollapsed);
	void setCanvasColor(const QColor &color);
	void setSelectItemAfterDrawing(bool enabled);
	void setControlsWidgetVisible(bool enabled);

Q_SIGNALS:
	void imageChanged() const;
	void currentTabChanged(int index) const;
	void tabCloseRequested(int index) const;
	void tabMoved(int fromIndex, int toIndex);
	void tabContextMenuOpened(int index) const;
	void currentImageReadyChanged(bool ready);
	void imageAdmissionChanged(bool allowed);
	void imageRequestFinished(quint64 requestId, const QImage &image, bool canceled, const QString &error);
	void rasterBackingError(const QString &message);
	void rasterBackingClosed();
	void tabCloseBatchRequested(const QList<int> &indices);

private:
	QScopedPointer<KImageAnnotatorPrivate> const d_ptr;
};

} // namespace kImageAnnotator

#endif // KIMAGEANNOTATOR_KIMAGEANNOTATOR_H
