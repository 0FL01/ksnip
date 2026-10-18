/*
 * Copyright (C) 2018 Damir Porobic <damir.porobic@gmx.com>
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

#include "CropCommand.h"

namespace kImageAnnotator {

CropCommand::CropCommand(QGraphicsPixmapItem *backgroundImage, const QRectF &cropRect, AnnotationArea *annotationArea) :
	mAnnotationArea(annotationArea),
	mBackgroundImage(backgroundImage),
	mOriginalImage(RasterImage::fromItem(backgroundImage, annotationArea->rasterDocument())),
	mCroppedImage(mOriginalImage.pixmap().copy(cropRect.toRect()), annotationArea->rasterDocument()),
	mNewItemOffset(cropRect.topLeft())
{
}

void CropCommand::undo()
{
	for (auto item : mAnnotationArea->items()) {
		auto baseItem = dynamic_cast<AbstractAnnotationItem *>(item);
		if (baseItem != nullptr) {
			baseItem->setPosition(baseItem->position() + mNewItemOffset);
		}
	}

	mOriginalImage.apply(mBackgroundImage);
	mAnnotationArea->setSceneRect(mBackgroundImage->boundingRect());
}

void CropCommand::redo()
{
	for (auto item : mAnnotationArea->items()) {
		auto baseItem = dynamic_cast<AbstractAnnotationItem *>(item);
		if (baseItem != nullptr) {
			baseItem->setPosition(baseItem->position() - mNewItemOffset);
		}
	}

	mCroppedImage.apply(mBackgroundImage);
	mAnnotationArea->setSceneRect(mBackgroundImage->boundingRect());
}

} // namespace kImageAnnotator
