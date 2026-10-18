/*
 * Copyright (C) 2020 Damir Porobic <damir.porobic@gmx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#ifndef ANNOTATIONTABWIDGET_H
#define ANNOTATIONTABWIDGET_H

#include <QTabWidget>
#include <QPixmap>

#include "AnnotationTabContent.h"
#include "AnnotationTabContextMenu.h"
#include "AnnotationTabCloser.h"
#include "AnnotationTabClickEventFilter.h"
#include "src/common/filter/KeyEventFilter.h"


namespace kImageAnnotator {

class AnnotationTabWidget : public QTabWidget
{
	Q_OBJECT
public:
	AnnotationTabWidget(Config *config, AbstractSettingsProvider *settingsProvider);
	~AnnotationTabWidget() override;
	int addTab(const QPixmap &image, const QString &title, const QString &toolTip);
	int insertTab(int index, const QPixmap &image, const QString &title, const QString &toolTip);
	AnnotationArea* currentAnnotationArea() const;
	AnnotationArea *annotationAreaAt(int index) const;
	ZoomValueProvider* currentZoomValueProvider() const;
	QAction* undoAction() const;
	QAction* redoAction() const;
	void updateTabInfo(int index, const QString &title, const QString &toolTip);
	void setUndoRedoEnabled(bool enabled);
    void addContextMenuActions(const QList<QAction*> & actions);
	bool enableRasterBacking(const QString &directory = QString());
	bool rasterBackingEnabled() const;
	bool isCurrentImageReady() const;
	bool canAcceptImage() const;
	bool reserveImageInput();
	void releaseImageInput();
	void closeRasterBacking();
	void requestImageAt(int index, quint64 requestId);
	void cancelImageRequest(quint64 requestId);
	void beginTabBatch();
	void endTabBatch();
	void removeTab(int index);
	QImage readyImageAt(int index) const;

signals:
	void imageChanged() const;
	void tabMoved(int fromIndex, int toIndex) const;
	void tabContextMenuOpened(int index) const;
	void currentImageReadyChanged(bool ready);
	void imageAdmissionChanged(bool allowed);
	void imageRequestFinished(quint64 requestId, const QImage &image, bool canceled, const QString &error);
	void rasterBackingError(const QString &message);
	void rasterBackingClosed();
	void tabCloseBatchRequested(const QList<int> &indices);

public slots:
	void undoTriggered() const;
	void redoTriggered() const;

protected:
	void tabInserted(int index) override;
	void tabRemoved(int index) override;

private:
	Config *mConfig;
	QTabBar *mTabBar;
	AbstractSettingsProvider *mSettingsProvider;
	QAction *mUndoAction;
	QAction *mRedoAction;
	AnnotationTabContextMenu *mTabContextMenu;
	AnnotationTabCloser *mTabCloser;
	AnnotationTabClickEventFilter *mTabClickFilter;
	AnnotationItemClipboard *mItemClipboard;
	KeyEventFilter mKeyEventFilter;
	RasterStore *mRasterStore = nullptr;
	QHash<AnnotationTabContent *, quint64> mOpenPages;
	QPointer<AnnotationTabContent> mResidentPage;
	QPointer<AnnotationTabContent> mDesiredPage;
	QPointer<AnnotationTabContent> mInteractivePage;
	QPointer<AnnotationTabContent> mPendingReleasePage;
	quint64 mNextGeneration = 0;
	int mTabBatchDepth = 0;
	bool mInserting = false;
	bool mDocumentsOpened = false;
	bool mPumpQueued = false;
	bool mStorageFailed = false;
	bool mImageInputReserved = false;
	bool mClosing = false;
	bool mLastReady = true;
	bool mLastAdmission = true;
	bool mUndoRedoEnabled = true;
	struct ImageRequest {
		QPointer<AnnotationTabContent> page;
		quint64 generation;
		quint64 serial;
		bool terminalQueued = false;
		bool canceled = false;
	};
	QHash<quint64, ImageRequest> mImageRequests;
	QList<quint64> mRequestQueue;
	struct PasteRequest {
		QPointer<AnnotationTabContent> page;
		quint64 generation = 0;
		quint64 clipboardGeneration = 0;
		QPointF position;
		bool pending = false;
	} mPasteRequest;

	bool isOpen(AnnotationTabContent *page, quint64 generation) const;
	AnnotationTabContent *selectedPage() const;
	void updateRasterState();
	void scheduleRasterWork();
	void runRasterWork();
	void failRasterWork(const QString &error);
	RasterHandles admissionHandles() const;
	void finishRequest(quint64 id, quint64 serial, const QImage &image, bool canceled, const QString &error);
	void requestPaste(AnnotationTabContent *page, const QPointF &position);

private slots:
	void updateCurrentWidget(int index);
	void showTabContextMenu(const QPoint &pos);
	void tabChanged();
};

} // namespace kImageAnnotator

#endif //ANNOTATIONTABWIDGET_H
