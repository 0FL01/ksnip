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

#include "AnnotationItemClipboard.h"

namespace kImageAnnotator {

AnnotationItemClipboard::AnnotationItemClipboard(QObject *parent) : QObject(parent) {}
AnnotationItemClipboard::~AnnotationItemClipboard() { clear(); }

void AnnotationItemClipboard::enableRasterBacking()
{
	Q_ASSERT(isEmpty());
	mRasterDocument = new RasterDocument(this);
	mRasterDocument->setResident(false);
}

RasterHandles AnnotationItemClipboard::rasterHandles() const
{
	return mRasterDocument ? mRasterDocument->handles() : RasterHandles();
}

quint64 AnnotationItemClipboard::generation() const { return mGeneration; }

bool AnnotationItemClipboard::isEmpty() const
{
	return mCopiedItemsToOffset.count() == 0;
}

bool AnnotationItemClipboard::isNotEmpty() const
{
	return !isEmpty();
}

void AnnotationItemClipboard::copyItems(const QPointF &position, const AnnotationItemModifier *itemModifier)
{
	clear();

	for (auto item : itemModifier->selectedItems()) {
		auto snapshot = AnnotationItemFactory::cloneItem(item);
		if (snapshot) {
			snapshot->bindRasterDocument(mRasterDocument);
			snapshot->setRasterEffectsResident(false);
			mCopiedItemsToOffset[snapshot] = item->position() - position;
		}
	}
}

QHash<AbstractAnnotationItem *, QPointF> AnnotationItemClipboard::copiedItemsWithOffset() const
{
	return mCopiedItemsToOffset;
}

void AnnotationItemClipboard::clear()
{
	++mGeneration;
	qDeleteAll(mCopiedItemsToOffset.keys());
	mCopiedItemsToOffset.clear();
}

} // namespace kImageAnnotator
