/*
 * Copyright (C) 2022 Damir Porobic <damir.porobic@gmx.com>
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

#include "CutCommand.h"

namespace kImageAnnotator {

CutCommand::CutCommand(QGraphicsPixmapItem *backgroundImage, const QRectF &cutOutRect, AnnotationArea *annotationArea) :
	mAnnotationArea(annotationArea),
	mBackgroundImage(backgroundImage),
	mOriginalImage(RasterImage::fromItem(backgroundImage, annotationArea->rasterDocument()))
{
	mCutImage.bind(annotationArea->rasterDocument());
	auto imageRect = backgroundImage->boundingRect();
	if(isVerticalCut(cutOutRect, imageRect)) {
		createCutImage(imageRect, cutOutRect.bottomLeft(), cutOutRect.topRight(), cutOutRect.topLeft());
	} else {
		createCutImage(imageRect, cutOutRect.topRight(), cutOutRect.bottomLeft(), cutOutRect.topLeft());
	}
}

void CutCommand::undo()
{
	mOriginalImage.apply(mBackgroundImage);
	mAnnotationArea->setSceneRect(mBackgroundImage->boundingRect());
}

void CutCommand::redo()
{
	mCutImage.apply(mBackgroundImage);
	mAnnotationArea->setSceneRect(mBackgroundImage->boundingRect());
}

bool CutCommand::isVerticalCut(const QRectF &cutOutRect, const QRectF &imageRect) const
{
	return imageRect.top() == cutOutRect.top() && imageRect.bottom() == cutOutRect.bottom();
}

void CutCommand::createCutImage(const QRectF &imageRect, const QPointF &bottomRightRect1, const QPointF &topLeftRect2, const QPointF &connectionPoint)
{
	auto rect1 = QRectF(imageRect.topLeft(), bottomRightRect1);
	auto rect2 = QRectF(topLeftRect2, imageRect.bottomRight());
	auto image1 = mOriginalImage.pixmap().copy(rect1.toRect());
	auto image2 = mOriginalImage.pixmap().copy(rect2.toRect());

	rect2.moveTopLeft(connectionPoint);

	QPixmap cutImage(rect1.united(rect2).size().toSize());
	cutImage.fill(Qt::transparent); // Partial/DPR-rounded copies must not expose uninitialized pixels.
	QPainter painter(&cutImage);
	painter.drawPixmap(rect1.topLeft(), image1);
	painter.drawPixmap(rect2.topLeft(), image2);
	painter.end();
	mCutImage = cutImage;
}

} // namespace kImageAnnotator
