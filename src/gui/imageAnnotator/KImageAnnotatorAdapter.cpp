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

#include "KImageAnnotatorAdapter.h"

KImageAnnotatorAdapter::KImageAnnotatorAdapter() :
	mKImageAnnotator(new KImageAnnotator)
{
	connect(mKImageAnnotator, &KImageAnnotator::imageChanged, this, &KImageAnnotatorAdapter::imageChanged);
	connect(mKImageAnnotator, &KImageAnnotator::currentTabChanged, this, &KImageAnnotatorAdapter::currentTabChanged);
	connect(mKImageAnnotator, &KImageAnnotator::tabCloseRequested, this, &KImageAnnotatorAdapter::tabCloseRequested);
	connect(mKImageAnnotator, &KImageAnnotator::tabMoved, this, &KImageAnnotatorAdapter::tabMoved);
	connect(mKImageAnnotator, &KImageAnnotator::tabContextMenuOpened, this, &KImageAnnotatorAdapter::tabContextMenuOpened);
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	connect(mKImageAnnotator, &KImageAnnotator::currentImageReadyChanged, this, &IImageAnnotator::currentImageReadyChanged);
	connect(mKImageAnnotator, &KImageAnnotator::imageAdmissionChanged, this, &IImageAnnotator::imageAdmissionChanged);
	connect(mKImageAnnotator, &KImageAnnotator::imageRequestFinished, this, &IImageAnnotator::imageRequestFinished);
	connect(mKImageAnnotator, &KImageAnnotator::rasterBackingError, this, &IImageAnnotator::rasterBackingError);
	connect(mKImageAnnotator, &KImageAnnotator::tabCloseBatchRequested, this, &IImageAnnotator::tabCloseBatchRequested);
	connect(mKImageAnnotator, &KImageAnnotator::rasterBackingClosed, this, [this]() {
		std::function<void()> finished;
		finished.swap(mBackingCloseFinished);
		if (finished) { finished(); }
	});
#endif
}

KImageAnnotatorAdapter::~KImageAnnotatorAdapter()
{
	delete mKImageAnnotator;
}

QImage KImageAnnotatorAdapter::image() const
{
	return mKImageAnnotator->image();
}

QImage KImageAnnotatorAdapter::imageAt(int index) const
{
	return mKImageAnnotator->imageAt(index);
}

bool KImageAnnotatorAdapter::enableRasterBacking(const QString &directory)
{
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	mBackingInitializationFailed = !mKImageAnnotator->enableRasterBacking(directory);
	return !mBackingInitializationFailed;
#else
	Q_UNUSED(directory)
	return false;
#endif
}

bool KImageAnnotatorAdapter::rasterBackingEnabled() const
{
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	return mKImageAnnotator->rasterBackingEnabled();
#else
	return false;
#endif
}

bool KImageAnnotatorAdapter::isCurrentImageReady() const
{
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	return mKImageAnnotator->isCurrentImageReady();
#else
	return true;
#endif
}

bool KImageAnnotatorAdapter::canAcceptImage() const
{
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	return !mBackingInitializationFailed && mKImageAnnotator->canAcceptImage();
#else
	return true;
#endif
}

bool KImageAnnotatorAdapter::reserveImageInput()
{
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	return !mBackingInitializationFailed && mKImageAnnotator->reserveImageInput();
#else
	return true;
#endif
}

void KImageAnnotatorAdapter::releaseImageInput()
{
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	mKImageAnnotator->releaseImageInput();
#endif
}

void KImageAnnotatorAdapter::requestImageAt(int index, quint64 requestId)
{
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	mKImageAnnotator->requestImageAt(index, requestId);
#else
	IImageAnnotator::requestImageAt(index, requestId);
#endif
}

void KImageAnnotatorAdapter::closeRasterBacking(const std::function<void()> &finished)
{
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	if (rasterBackingEnabled()) {
		mBackingCloseFinished = finished;
		mKImageAnnotator->closeRasterBacking();
		return;
	}
#endif
	IImageAnnotator::closeRasterBacking(finished);
}

void KImageAnnotatorAdapter::cancelImageRequest(quint64 requestId)
{
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	mKImageAnnotator->cancelImageRequest(requestId);
#else
	Q_UNUSED(requestId)
#endif
}

void KImageAnnotatorAdapter::beginTabBatch()
{
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	mKImageAnnotator->beginTabBatch();
#endif
}

void KImageAnnotatorAdapter::endTabBatch()
{
#ifdef KIMAGEANNOTATOR_HAS_RASTER_BACKING
	mKImageAnnotator->endTabBatch();
#endif
}

QAction *KImageAnnotatorAdapter::undoAction()
{
	return mKImageAnnotator->undoAction();
}

QAction *KImageAnnotatorAdapter::redoAction()
{
	return mKImageAnnotator->redoAction();
}

QSize KImageAnnotatorAdapter::sizeHint() const
{
	return mKImageAnnotator->sizeHint();
}

void KImageAnnotatorAdapter::showAnnotator()
{
	mKImageAnnotator->showAnnotator();
}

void KImageAnnotatorAdapter::showCropper()
{
	mKImageAnnotator->showCropper();
}

void KImageAnnotatorAdapter::showScaler()
{
	mKImageAnnotator->showScaler();
}

void KImageAnnotatorAdapter::setSettingsCollapsed(bool isCollapsed)
{
	mKImageAnnotator->setSettingsCollapsed(isCollapsed);
}

void KImageAnnotatorAdapter::hide()
{
	mKImageAnnotator->hide();
}

void KImageAnnotatorAdapter::close()
{
	mKImageAnnotator->close();
}

bool KImageAnnotatorAdapter::isVisible() const
{
	return mKImageAnnotator->isVisible();
}

QWidget *KImageAnnotatorAdapter::widget() const
{
	return mKImageAnnotator;
}

void KImageAnnotatorAdapter::loadImage(const QPixmap &pixmap)
{
	mKImageAnnotator->loadImage(pixmap);
}

int KImageAnnotatorAdapter::addTab(const QPixmap &pixmap, const QString &title, const QString &toolTip)
{
	return mKImageAnnotator->addTab(pixmap, title, toolTip);
}

void KImageAnnotatorAdapter::updateTabInfo(int index, const QString &title, const QString &toolTip)
{
	mKImageAnnotator->updateTabInfo(index, title, toolTip);
}

void KImageAnnotatorAdapter::insertImageItem(const QPointF &position, const QPixmap &pixmap)
{
	mKImageAnnotator->insertImageItem(position, pixmap);
}

void KImageAnnotatorAdapter::setSmoothPathEnabled(bool enabled)
{
	mKImageAnnotator->setSmoothPathEnabled(enabled);
}

void KImageAnnotatorAdapter::setSaveToolSelection(bool enabled)
{
	mKImageAnnotator->setSaveToolSelection(enabled);
}

void KImageAnnotatorAdapter::setSmoothFactor(int factor)
{
	mKImageAnnotator->setSmoothFactor(factor);
}

void KImageAnnotatorAdapter::setSwitchToSelectToolAfterDrawingItem(bool enabled)
{
	mKImageAnnotator->setSwitchToSelectToolAfterDrawingItem(enabled);
}

void KImageAnnotatorAdapter::setSelectItemAfterDrawing(bool enabled)
{
	mKImageAnnotator->setSelectItemAfterDrawing(enabled);
}

void KImageAnnotatorAdapter::setNumberToolSeedChangeUpdatesAllItems(bool enabled)
{
	mKImageAnnotator->setNumberToolSeedChangeUpdatesAllItems(enabled);
}

void KImageAnnotatorAdapter::setTabBarAutoHide(bool enabled)
{
	mKImageAnnotator->setTabBarAutoHide(enabled);
}

void KImageAnnotatorAdapter::removeTab(int index)
{
	mKImageAnnotator->removeTab(index);
}

void KImageAnnotatorAdapter::setStickers(const QStringList &stickerPaths, bool keepDefault)
{
	mKImageAnnotator->setStickers(stickerPaths, keepDefault);
}

void KImageAnnotatorAdapter::addTabContextMenuActions(const QList<QAction *> &actions)
{
	mKImageAnnotator->addTabContextMenuActions(actions);
}

void KImageAnnotatorAdapter::showCanvasModifier()
{
	mKImageAnnotator->showCanvasModifier();
}

void KImageAnnotatorAdapter::showRotator()
{
	mKImageAnnotator->showRotator();
}

void KImageAnnotatorAdapter::showCutter()
{
	mKImageAnnotator->showCutter();
}

void KImageAnnotatorAdapter::setCanvasColor(const QColor &color)
{
	mKImageAnnotator->setCanvasColor(color);
}

void KImageAnnotatorAdapter::setControlsWidgetVisible(bool isVisible)
{
	mKImageAnnotator->setControlsWidgetVisible(isVisible);
}
