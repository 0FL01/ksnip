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

#include "AnnotationArea.h"

namespace kImageAnnotator {

AnnotationArea::AnnotationArea(
		Config *config,
		AbstractSettingsProvider *settingsProvider,
		IDevicePixelRatioScaler *devicePixelRatioScaler,
		ZoomValueProvider *zoomValueProvider,
		AnnotationItemClipboard *itemClipboard,
		QWidget *parent) :
	QGraphicsScene(parent),
	mUndoStack(new UndoStack),
	mBackgroundImage(nullptr),
	mCurrentItem(nullptr),
	mUndoAction(nullptr),
	mRedoAction(nullptr),
	mConfig(config),
	mKeyHelper(new KeyHelper),
	mSettingsProvider(settingsProvider),
	mItemModifier(new AnnotationItemModifier(zoomValueProvider)),
	mPropertiesFactory(new AnnotationPropertiesFactory(config, mSettingsProvider)),
	mItemFactory(new AnnotationItemFactory(mPropertiesFactory, mSettingsProvider, mConfig)),
	mItems(new QList<AbstractAnnotationItem *>()),
	mItemClipboard(itemClipboard),
	mDevicePixelRatioScaler(devicePixelRatioScaler),
	mCanvasColor(config->canvasColor())
{
	Q_ASSERT(mSettingsProvider != nullptr);
	Q_ASSERT(mConfig != nullptr);

	addItem(mItemModifier);

	connect(mItemModifier, &AnnotationItemModifier::newCommand, mUndoStack, &UndoStack::push);
	connect(mItemModifier, &AnnotationItemModifier::itemsSelected, this, &AnnotationArea::itemsSelected);
	connect(mItemModifier, &AnnotationItemModifier::itemsDeselected, this, &AnnotationArea::itemsDeselected);
	connect(mItemModifier, &AnnotationItemModifier::itemModified, this, &AnnotationArea::imageChanged);
	connect(mUndoStack, &UndoStack::indexChanged, this, [this]() {
		update();
		// In backing mode asynchronous saves also track undo/redo as edits, not
		// merely the initial command push. Hydration never changes this index.
		if (mRasterDocument) { emit imageChanged(); }
	});
	connect(mKeyHelper, &KeyHelper::deleteReleased, this, &AnnotationArea::deleteSelectedItems);
	connect(mKeyHelper, &KeyHelper::escapeReleased, mItemModifier, &AnnotationItemModifier::clear);

	connect(mKeyHelper, &KeyHelper::undoPressed, mUndoStack, &UndoStack::undo);
	connect(mKeyHelper, &KeyHelper::redoPressed, mUndoStack, &UndoStack::redo);
}

AnnotationArea::~AnnotationArea()
{
	// QUndoStack::clear() emits indexChanged during destruction; it is no longer
	// an edit and must not call into an already dismantling tab/widget hierarchy.
	disconnect(mUndoStack, nullptr, this, nullptr);
	delete mUndoStack;
	const auto ownedItems = mOwnedItems.values();
	for (const auto &item : ownedItems) {
		if (item) { delete item; }
	}
	delete mPropertiesFactory;
	delete mItemFactory;
	delete mItems;
	delete mKeyHelper;
	delete mItemModifier;
	delete mDevicePixelRatioScaler;
}

void AnnotationArea::loadImage(const QPixmap &image)
{
	if (image.isNull()) {
		return;
	}

	resetAnnotationArea();
	replaceBackgroundImage(image);
	imageEffectChanged(mSettingsProvider->effect());
}

void AnnotationArea::insertImageItem(const QPointF &position, const QPixmap &image)
{
	if (!mRasterInteractionEnabled) {
		return;
	}
	auto imageItem = mItemFactory->create(position, image);
	mUndoStack->push(new AddCommand(imageItem, this));
	toolChanged(mSettingsProvider->toolType());
}

void AnnotationArea::replaceBackgroundImage(const QPixmap &image)
{
	if (mRasterDocument) {
		auto item = new RasterPixmapItem(image, mRasterDocument);
		addItem(item);
		mBackgroundImage.reset(item);
	} else {
		mBackgroundImage = QSharedPointer<QGraphicsPixmapItem>(addPixmap(image));
	}
	setSceneRect(QRect());
}

QImage AnnotationArea::image()
{
	if (mBackgroundImage == nullptr || !isRasterReady()) {
		return {};
	}

	auto modifierVisible = mItemModifier->isVisible();
	// Remember a null (automatic) scene rect as null, rather than making its
	// current computed bounds explicit and preventing future item expansion.
	auto previousSceneRect = mRequestedSceneRect;
	if (mRasterDocument) {
		mItemModifier->setVisible(false);
	} else {
		mItemModifier->clear();
	}
	
	setSceneRect(canvasRect());
	auto sceneRect = this->sceneRect().toRect();
	QImage image(sceneRect.size(), QImage::Format_ARGB32_Premultiplied);
	image.fill(mCanvasColor);

	if (mRasterDocument) {
		// QGraphicsScene::render keeps item-discovery bits until its outer paint
		// ends. Computing a lazy mask inside that paint would miss the background
		// during its nested render. Prepare pending masks before entering it.
		for (auto item : items(Qt::AscendingOrder)) {
			if (auto mask = dynamic_cast<AbstractAnnotationObfuscate *>(item)) {
				if (mask->isVisible()) { mask->prepareForRender(); }
			}
		}
	}

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	render(&painter, QRectF(), sceneRect);

	if (mRasterDocument) {
		setSceneRect(previousSceneRect);
		mItemModifier->setVisible(modifierVisible);
	} else {
		setSceneRect(QRect()); // Reset scene rect
	}

	return image;
}

QAction *AnnotationArea::undoAction()
{
	if(mUndoAction == nullptr) {
		mUndoAction = mUndoStack->createUndoAction(this);
	}
	return mUndoAction;
}

QAction *AnnotationArea::redoAction()
{
	if(mRedoAction == nullptr) {
		mRedoAction = mUndoStack->createRedoAction(this);
	}
	return mRedoAction;
}

void AnnotationArea::addAnnotationItem(AbstractAnnotationItem *item)
{
	if (!mOwnedItems.contains(item)) {
		mOwnedItems.insert(item, item);
		item->QObject::setParent(this);
		connect(item, &QObject::destroyed, this, [this, item]() { mOwnedItems.remove(item); });
	}
	item->bindRasterDocument(mRasterDocument);
	mItems->prepend(item);
	addItem(item);

	emit imageChanged();
}

void AnnotationArea::removeAnnotationItem(AbstractAnnotationItem *item)
{
	removeItem(item);
	mItems->removeOne(item);

	emit imageChanged();
}

void AnnotationArea::crop(const QRectF &rect)
{
	if (!mRasterInteractionEnabled) { return; }
	auto scaledRect = mDevicePixelRatioScaler->scale(rect);
	mUndoStack->push(new CropCommand(mBackgroundImage.data(), scaledRect, this));

	emit imageChanged();
}

void AnnotationArea::scale(const QSize &size)
{
	if (!mRasterInteractionEnabled) { return; }
	mUndoStack->push(new ScaleCommand(mBackgroundImage.data(), size, this));

	emit imageChanged();
}

void AnnotationArea::rotate(qreal angel)
{
	if (!mRasterInteractionEnabled) { return; }
	mUndoStack->push(new RotateCommand(mBackgroundImage.data(), angel, this));

	emit imageChanged();
}

void AnnotationArea::flip(FlipDirection direction)
{
	if (!mRasterInteractionEnabled) { return; }
	mUndoStack->push(new FlipCommand(mBackgroundImage.data(), direction, mRasterDocument));

	emit imageChanged();
}

void AnnotationArea::clearSelection()
{
	mItemModifier->clear();
	QGraphicsScene::clearSelection();
}

void AnnotationArea::itemSettingsChanged()
{
	if (!mRasterInteractionEnabled) { return; }
	auto selectedItems = mItemModifier->selectedItems();
	if(selectedItems.count() == 1) {
		auto item = selectedItems.first();
		auto properties = mPropertiesFactory->create(item->toolType());
		mUndoStack->push(new ChangePropertiesCommand(item, properties));
	}
}

void AnnotationArea::numberToolSeedChanged(int numberToolSeed)
{
	mItemFactory->setNumberToolSeed(numberToolSeed);
}

int AnnotationArea::numberToolSeed() const
{
	return mItemFactory->numberToolSeed();
}

void AnnotationArea::imageEffectChanged(ImageEffects effect)
{
	if (!mRasterInteractionEnabled || !mBackgroundImage) { return; }
	mImageEffect = effect;
	auto graphicsEffect = ImageEffectFactory::create(effect);
	mBackgroundImage->setGraphicsEffect(graphicsEffect);

	emit imageChanged();
}

void AnnotationArea::setCanvasRect(const QRectF &rect)
{
	mCanvasRect = rect;
}

void AnnotationArea::setSceneRect(const QRectF &rect)
{
	mRequestedSceneRect = rect;
	QGraphicsScene::setSceneRect(rect);
}

void AnnotationArea::setSceneRect(qreal x, qreal y, qreal width, qreal height)
{
	setSceneRect(QRectF(x, y, width, height));
}

QRectF AnnotationArea::canvasRect() const
{
	if(!isCustomCanvasRect()) {
		auto backgroundImageRect = mBackgroundImage->graphicsEffect()->boundingRect();
		return annotationItemsBoundingRect().united(backgroundImageRect);
	} else {
		return mCanvasRect;
	}
}

bool AnnotationArea::isCustomCanvasRect() const
{
	return !mCanvasRect.isNull();
}

void AnnotationArea::setCanvasColor(const QColor &color)
{
	mCanvasColor = color;
}

QColor AnnotationArea::canvasColor() const
{
	return mCanvasColor;
}

void AnnotationArea::modifyCanvas(const QRectF &canvasRect, const QColor &color)
{
	if (!mRasterInteractionEnabled) { return; }
	mUndoStack->push(new ModifyCanvasCommand(canvasRect, color, this));

	emit imageChanged();
}

QRectF AnnotationArea::backgroundImageRect() const
{
	return mBackgroundImage->boundingRect();
}

void AnnotationArea::cut(const QRectF &rect)
{
	if (!mRasterInteractionEnabled) { return; }
	auto scaledRect = mDevicePixelRatioScaler->scale(rect);
	mUndoStack->push(new CutCommand(mBackgroundImage.data(), scaledRect, this));

	emit imageChanged();
}

void AnnotationArea::keyPressed(QKeyEvent *keyEvent)
{
	if (!mRasterInteractionEnabled) { return; }
	mKeyHelper->keyPress(keyEvent);
}

void AnnotationArea::keyReleased(QKeyEvent *keyEvent)
{
	if (!mRasterInteractionEnabled) { return; }
	mKeyHelper->keyRelease(keyEvent);
}

void AnnotationArea::update()
{
	mItemModifier->updateSelection();
	QGraphicsScene::update();
}

void AnnotationArea::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (!mRasterInteractionEnabled) { event->accept(); return; }
	if (event->button() == Qt::LeftButton) {
		if (mSettingsProvider->toolType() == Tools::Select) {
			mItemModifier->handleMousePress(event->scenePos(), mItems, mKeyHelper->isControlPressed());
		} else {
			mItemModifier->clear();
			addItemAtPosition(event->scenePos());
		}
	}

	QGraphicsScene::mousePressEvent(event);
}

void AnnotationArea::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (!mRasterInteractionEnabled) { event->accept(); return; }
	if (event->buttons() == Qt::LeftButton) {
		if (mCurrentItem) {
			addPointToCurrentItem(event->scenePos());
		} else {
			mItemModifier->handleMouseMove(event->scenePos(), mKeyHelper->isControlPressed());
		}
	}

	QGraphicsScene::mouseMoveEvent(event);
}

void AnnotationArea::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (!mRasterInteractionEnabled) { event->accept(); return; }
	if (event->button() == Qt::LeftButton) {
		if (mSettingsProvider->toolType() == Tools::Select) {
			mItemModifier->handleMouseRelease(mItems);
		}
		finishDrawingItem(event->scenePos());
	}

	QGraphicsScene::mouseReleaseEvent(event);
}

void AnnotationArea::finishDrawingItem(const QPointF &pos)
{
	if(mCurrentItem != nullptr) {
		mCurrentItem->finish();

		auto isSelectToolAfterDrawingEnabled = mConfig->switchToSelectToolAfterDrawingItem() && mConfig->selectItemAfterDrawing();

		if (mCurrentItem->requiresSelectionAfterCreation() || isSelectToolAfterDrawingEnabled) {
			mItemModifier->selectItem(mCurrentItem);
		} else if (mConfig->switchToSelectToolAfterDrawingItem()) {
			mSettingsProvider->activateSelectTool();
		}
		mCurrentItem = nullptr;
	}
}

QRectF AnnotationArea::annotationItemsBoundingRect() const
{
	QRectF boundingRect;
	for(auto item : *mItems) {
		boundingRect = boundingRect.united(item->boundingRect());
	}
	return boundingRect;
}

void AnnotationArea::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	if (!mRasterInteractionEnabled) { event->accept(); return; }
	if (event->button() == Qt::LeftButton && mSettingsProvider->toolType() == Tools::Select) {
		mItemModifier->handleMouseDoubleClick(event->scenePos(), mItems);
	}
}

void AnnotationArea::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
	if (!mRasterInteractionEnabled) { event->accept(); return; }
	finishDrawingItem(event->scenePos());

	mItemModifier->handleSelectionAt(event->scenePos(), mItems, mKeyHelper->isControlPressed());
	auto selectedItems = mItemModifier->selectedItems();

	AnnotationContextMenu contextMenu;
	auto isMenuOverItem = !selectedItems.isEmpty();
	contextMenu.setOverItem(isMenuOverItem);
	contextMenu.setPastEnabled(mItemClipboard->isNotEmpty());
	contextMenu.setEditVisible(selectedEditableItem() != nullptr);
	AnnotationItemArranger itemArranger(selectedItems, mItems);
	connect(&itemArranger, &AnnotationItemArranger::newCommand, mUndoStack, &UndoStack::push);
	connect(&contextMenu, &AnnotationContextMenu::bringToFront, &itemArranger, &AnnotationItemArranger::bringToFront);
	connect(&contextMenu, &AnnotationContextMenu::bringForward, &itemArranger, &AnnotationItemArranger::bringForward);
	connect(&contextMenu, &AnnotationContextMenu::sendBackward, &itemArranger, &AnnotationItemArranger::sendBackward);
	connect(&contextMenu, &AnnotationContextMenu::sendToBack, &itemArranger, &AnnotationItemArranger::sendToBack);
	connect(&contextMenu, &AnnotationContextMenu::copy, [this](const QPointF &position) { mItemClipboard->copyItems(position, mItemModifier); } );
	connect(&contextMenu, &AnnotationContextMenu::paste, this, &AnnotationArea::pasteCopiedItems);
	connect(&contextMenu, &AnnotationContextMenu::erase, this, &AnnotationArea::deleteSelectedItems);
	connect(&contextMenu, &AnnotationContextMenu::edit, this, &AnnotationArea::enableEditing);

	contextMenu.exec(event->screenPos());
}

void AnnotationArea::addItemAtPosition(const QPointF &position)
{
	mCurrentItem = mItemFactory->create(position);
	mUndoStack->push(new AddCommand(mCurrentItem, this));
}

void AnnotationArea::addPointToCurrentItem(const QPointF &position)
{
	mCurrentItem->addPoint(position, mKeyHelper->isControlPressed());
}

void AnnotationArea::toolChanged(Tools toolType)
{
	auto isSelectTool = toolType == Tools::Select;

	for (auto item : *mItems) {
		if (isSelectTool) {
			item->setCursor(CursorHelper::movableCursor());
		} else {
			item->unsetCursor();
		}
	}

	if (!isSelectTool) {
		mItemModifier->clear();
	}
}

void AnnotationArea::resetAnnotationArea()
{
	removeAllItems();
	mItemModifier->clear();
	mUndoStack->clear();
	mItemFactory->reset();
	mKeyHelper->reset();
}

void AnnotationArea::removeAllItems()
{
	const auto items = *mItems;
	for (auto item : items) {
		removeAnnotationItem(item);
	}
}

void AnnotationArea::deleteSelectedItems()
{
	if (!mRasterInteractionEnabled) { return; }
	auto selectedItems = mItemModifier->selectedItems();
	mItemModifier->clear();
	mUndoStack->push(new DeleteCommand(selectedItems, this));
	if (mSettingsProvider->toolType() == Tools::Select) {
		mSettingsProvider->activateSelectTool();
	}
}

void AnnotationArea::pasteCopiedItems(const QPointF &position)
{
	if (!mRasterInteractionEnabled) { return; }
	if (mRasterDocument) {
		emit rasterPasteRequested(position);
	} else {
		syncPaste(position);
	}
}

void AnnotationArea::syncPaste(const QPointF &position)
{
	auto copiedItems = mItemClipboard->copiedItemsWithOffset();
	mUndoStack->push(new PasteCommand(copiedItems, position, mItemFactory, this));
}

void AnnotationArea::enableRasterBacking()
{
	Q_ASSERT(!mBackgroundImage);
	if (!mRasterDocument) {
		mRasterDocument = new RasterDocument(this);
	}
}

RasterDocument *AnnotationArea::rasterDocument() const { return mRasterDocument; }

bool AnnotationArea::isRasterReady() const
{
	return !mRasterDocument || (mRasterDocument->isResident() && mRasterDocument->isDecoded());
}

void AnnotationArea::setRasterInteractionEnabled(bool enabled)
{
	mRasterInteractionEnabled = enabled;
	if (!enabled) {
		mKeyHelper->reset();
	}
	if (mUndoAction) { mUndoAction->setEnabled(enabled && mUndoStack->canUndo()); }
	if (mRedoAction) { mRedoAction->setEnabled(enabled && mUndoStack->canRedo()); }
}

void AnnotationArea::setRasterResident(bool resident)
{
	if (!mRasterDocument) { return; }
	mRasterDocument->setResident(resident);
	if (auto item = dynamic_cast<RasterPixmapItem *>(mBackgroundImage.data())) {
		item->refreshDecodedPixmap();
		// Invalidate Qt's hidden source-pixmap cache without replacing the effect
		// (its bounds remain document metadata while the page is cold).
		if (auto effect = item->graphicsEffect()) {
			auto enabled = effect->isEnabled();
			effect->setEnabled(!enabled);
			effect->setEnabled(enabled);
		}
		item->setCacheMode(QGraphicsItem::NoCache);
	}
	for (const auto &item : mOwnedItems) {
		if (item) { item->setRasterEffectsResident(resident); }
	}
	QGraphicsScene::update();
}

void AnnotationArea::enableEditing()
{
    auto editableItem = selectedEditableItem();
    if(editableItem != nullptr) {
        mItemModifier->clear();
        editableItem->enableEditing();
        itemsDeselected();
    }
}

EditableItem* AnnotationArea::selectedEditableItem() const
{
    auto selectedItems = mItemModifier->selectedItems();
    return selectedItems.length() != 1 ? nullptr : dynamic_cast<EditableItem *>(selectedItems[0]);
}

void AnnotationArea::itemsSelected(const QList<AbstractAnnotationItem *> &items) const
{
	if(items.count() != 1) {
		mSettingsProvider->activateSelectTool();
		return;
	}
	auto item = items.first();
	mSettingsProvider->editItem(item);
}

void AnnotationArea::itemsDeselected()
{
	mSettingsProvider->activateSelectTool();
}

void AnnotationArea::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
	Q_UNUSED(event)

	// Overriding default dragMoveEvent in order to accept drops without items under them
}

} // namespace kImageAnnotator
