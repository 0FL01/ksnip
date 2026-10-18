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

#include "AnnotationTabWidget.h"

namespace kImageAnnotator {

AnnotationTabWidget::AnnotationTabWidget(Config *config, AbstractSettingsProvider *settingsProvider) :
	mConfig(config),
	mTabBar(tabBar()),
	mSettingsProvider(settingsProvider),
	mUndoAction(new QAction(this)),
	mRedoAction(new QAction(this)),
	mTabContextMenu(new AnnotationTabContextMenu(this)),
	mTabCloser(new AnnotationTabCloser(this)),
	mTabClickFilter(new AnnotationTabClickEventFilter(mTabBar, this)),
	mItemClipboard(new AnnotationItemClipboard(this))
{
	setTabBarAutoHide(true);
	setMovable(true);
	setTabsClosable(true);
	mTabBar->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(mUndoAction, &QAction::triggered, this, &AnnotationTabWidget::undoTriggered);
	connect(mRedoAction, &QAction::triggered, this, &AnnotationTabWidget::redoTriggered);
	connect(mTabBar, &QTabBar::tabMoved, this, &AnnotationTabWidget::tabMoved);
	connect(mTabBar, &QTabBar::customContextMenuRequested, this, &AnnotationTabWidget::showTabContextMenu);
	connect(mTabBar, &QTabBar::currentChanged, this, &AnnotationTabWidget::tabChanged);

	connect(mTabContextMenu, &AnnotationTabContextMenu::closeTab, mTabCloser, &AnnotationTabCloser::closeTabTriggered);
	connect(mTabContextMenu, &AnnotationTabContextMenu::closeOtherTabs, mTabCloser, &AnnotationTabCloser::closeOtherTabsTriggered);
	connect(mTabContextMenu, &AnnotationTabContextMenu::closeAllTabs, mTabCloser, &AnnotationTabCloser::closeAllTabsTriggered);
	connect(mTabContextMenu, &AnnotationTabContextMenu::closeAllTabsToLeft, mTabCloser, &AnnotationTabCloser::closeAllTabsToLeftTriggered);
	connect(mTabContextMenu, &AnnotationTabContextMenu::closeAllTabsToRight, mTabCloser, &AnnotationTabCloser::closeAllTabsToRightTriggered);

	connect(mTabClickFilter, &AnnotationTabClickEventFilter::closeTabTriggered, mTabCloser, &AnnotationTabCloser::closeTabTriggered);
	connect(mTabCloser, &AnnotationTabCloser::tabCloseBatchRequested, this, &AnnotationTabWidget::tabCloseBatchRequested);
}

AnnotationTabWidget::~AnnotationTabWidget()
{
	disconnect(mTabBar, nullptr, this, nullptr);
	disconnect(this, nullptr, nullptr, nullptr);
	mKeyEventFilter.setListener(nullptr);
	mSettingsProvider->setActiveListener(nullptr);
	mSettingsProvider->setActiveZoomValueProvider(nullptr);
	// Stop GUI consumers first. Worker jobs retain only immutable handles and
	// session ownership; they do not retain or dereference a tab/scene/widget.
	for (auto page : mOpenPages.keys()) {
		disconnect(page->annotationArea(), nullptr, this, nullptr);
		delete page;
	}
	mOpenPages.clear();
	delete mItemClipboard;
	mItemClipboard = nullptr;
	delete mRasterStore;
}

int AnnotationTabWidget::addTab(const QPixmap &image, const QString &title, const QString &toolTip)
{
	return insertTab(count(), image, title, toolTip);
}

int AnnotationTabWidget::insertTab(int index, const QPixmap &image, const QString &title, const QString &toolTip)
{
	if (mRasterStore && ((!mImageInputReserved && !canAcceptImage()) || image.isNull())) {
		emit rasterBackingError(tr("A screenshot cannot be admitted until the previous raster operation is safe."));
		return -1;
	}
	mImageInputReserved = false; // Consume at most one previously granted credit.
	// A granted input remains usable (including its cursor) after an earlier
	// failure. Retry persistence once; another failure freezes further editing.
	mStorageFailed = false;
	if (mRasterStore && mResidentPage) {
		mResidentPage->setRasterReady(false);
		mResidentPage->annotationArea()->setRasterResident(false);
		mResidentPage = nullptr;
		updateRasterState();
	}
	auto content = new AnnotationTabContent(image, mConfig, mSettingsProvider, mItemClipboard, mRasterStore != nullptr);
	mDocumentsOpened = true;
	mOpenPages.insert(content, ++mNextGeneration);
	connect(content->annotationArea(), &AnnotationArea::imageChanged, this, [this]() {
		// An explicit edit is also an explicit retry opportunity after a storage
		// failure. There is no timer, idle polling or automatic retry loop.
		mStorageFailed = false;
		emit imageChanged();
		scheduleRasterWork();
	});
	connect(content->annotationArea(), &AnnotationArea::rasterPasteRequested, this,
			[this, content](const QPointF &position) { requestPaste(content, position); });
	if (mRasterStore) {
		connect(content->annotationArea()->rasterDocument(), &RasterDocument::resourcesChanged,
				this, &AnnotationTabWidget::scheduleRasterWork, Qt::QueuedConnection);
	}
	mInserting = true;
	auto newTabIndex = QTabWidget::insertTab(index, content, title);
	setTabToolTip(newTabIndex, toolTip);
	mInserting = false;
	if (mRasterStore) {
		mResidentPage = content;
		mDesiredPage = content;
	}
	tabChanged();
	scheduleRasterWork();
	return newTabIndex;
}

AnnotationArea* AnnotationTabWidget::currentAnnotationArea() const
{
	return currentWidget() != nullptr ? dynamic_cast<AnnotationTabContent*>(currentWidget())->annotationArea() : nullptr;
}

AnnotationArea *AnnotationTabWidget::annotationAreaAt(int index) const
{
	return widget(index) != nullptr ? dynamic_cast<AnnotationTabContent*>(widget(index))->annotationArea() : nullptr;
}

ZoomValueProvider *AnnotationTabWidget::currentZoomValueProvider() const
{
	return currentWidget() != nullptr ? dynamic_cast<AnnotationTabContent *>(currentWidget())->zoomValueProvider() : nullptr;
}

QAction *AnnotationTabWidget::undoAction() const
{
	return mUndoAction;
}

QAction *AnnotationTabWidget::redoAction() const
{
	return mRedoAction;
}

void AnnotationTabWidget::updateTabInfo(int index, const QString &title, const QString &toolTip)
{
	setTabText(index, title);
	setTabToolTip(index, toolTip);
}

void AnnotationTabWidget::setUndoRedoEnabled(bool enabled)
{
	mUndoRedoEnabled = enabled;
	mUndoAction->setEnabled(enabled && isCurrentImageReady());
	mRedoAction->setEnabled(enabled && isCurrentImageReady());
}

void AnnotationTabWidget::tabInserted(int index)
{
	updateCurrentWidget(index);
	QTabWidget::tabInserted(index);
}

void AnnotationTabWidget::tabRemoved(int index)
{
	QTabWidget::tabRemoved(index);
}

void AnnotationTabWidget::undoTriggered() const
{
	auto annotationArea = currentAnnotationArea();
	if(annotationArea != nullptr && isCurrentImageReady()) {
		annotationArea->undoAction()->trigger();
	}
}

void AnnotationTabWidget::redoTriggered() const
{
	auto annotationArea = currentAnnotationArea();
	if(annotationArea != nullptr && isCurrentImageReady()) {
		annotationArea->redoAction()->trigger();
	}
}

void AnnotationTabWidget::updateCurrentWidget(int index)
{
	setCurrentIndex(index);
}

void AnnotationTabWidget::showTabContextMenu(const QPoint &pos)
{
	if (!pos.isNull()) {
		int tabIndex = mTabBar->tabAt(pos);
		emit tabContextMenuOpened(tabIndex);
		mTabContextMenu->show(tabIndex, mTabBar->mapToGlobal(pos));
	}
}

void AnnotationTabWidget::tabChanged()
{
	if (mInserting) { return; }
	if (mClosing) { return; }
	if (mRasterStore) {
		mDesiredPage = selectedPage();
		if (mStorageFailed && mDesiredPage != mResidentPage) { mStorageFailed = false; }
		if (mPasteRequest.pending && mPasteRequest.page != mDesiredPage) {
			mPasteRequest.pending = false;
		}
		updateRasterState();
		scheduleRasterWork();
		return;
	}
	mSettingsProvider->setActiveListener(currentAnnotationArea());
	mSettingsProvider->setActiveZoomValueProvider(currentZoomValueProvider());
	mKeyEventFilter.setListener(currentAnnotationArea());
}

bool AnnotationTabWidget::enableRasterBacking(const QString &directory)
{
	if (mRasterStore) { return true; }
	if (mDocumentsOpened) {
		emit rasterBackingError(tr("Screenshot backing must be enabled before opening documents."));
		return false;
	}
	auto store = new RasterStore(this);
	QString error;
	if (!store->initialize(directory, &error)) {
		delete store;
		emit rasterBackingError(error);
		return false;
	}
	mRasterStore = store;
	mItemClipboard->enableRasterBacking();
	mTabCloser->setBatchCloseEnabled(true);
	updateRasterState();
	return true;
}

bool AnnotationTabWidget::rasterBackingEnabled() const { return mRasterStore != nullptr; }

AnnotationTabContent *AnnotationTabWidget::selectedPage() const
{
	return dynamic_cast<AnnotationTabContent *>(currentWidget());
}

bool AnnotationTabWidget::isOpen(AnnotationTabContent *page, quint64 generation) const
{
	return page && mOpenPages.value(page, 0) == generation;
}

bool AnnotationTabWidget::isCurrentImageReady() const
{
	if (mClosing) { return false; }
	if (!mRasterStore) { return true; }
	auto page = selectedPage();
	return page && page == mResidentPage && mOpenPages.contains(page) &&
		page->annotationArea()->isRasterReady() && !mPasteRequest.pending;
}

RasterHandles AnnotationTabWidget::admissionHandles() const
{
	auto handles = mItemClipboard->rasterHandles();
	if (mResidentPage) { handles.append(mResidentPage->annotationArea()->rasterDocument()->handles()); }
	// A previously admitted capture may arrive after persistence failed. Such a
	// transition retains the old last RAM copy until an explicit retry succeeds.
	for (auto page : mOpenPages.keys()) {
		if (page == mResidentPage) { continue; }
		for (const auto &handle : page->annotationArea()->rasterDocument()->handles()) {
			if (!handle->blob) { handles.append(handle); }
		}
	}
	return handles;
}

bool AnnotationTabWidget::canAcceptImage() const
{
	if (mClosing) { return false; }
	if (!mRasterStore) { return true; }
	if (mImageInputReserved || mStorageFailed || mRasterStore->isBusy() || mTabBatchDepth || mPasteRequest.pending || mPendingReleasePage ||
		mDesiredPage != mResidentPage) {
		return false;
	}
	for (const auto &handle : admissionHandles()) {
		if (!handle->blob) { return false; }
	}
	return true;
}

bool AnnotationTabWidget::reserveImageInput()
{
	if (!canAcceptImage()) { return false; }
	if (mRasterStore) {
		mImageInputReserved = true;
		updateRasterState();
	}
	return true;
}

void AnnotationTabWidget::releaseImageInput()
{
	mImageInputReserved = false;
	scheduleRasterWork();
}

void AnnotationTabWidget::closeRasterBacking()
{
	if (mClosing) { return; }
	mClosing = true;
	mPasteRequest.pending = false;
	const auto requests = mImageRequests.keys();
	for (auto request : requests) { cancelImageRequest(request); }
	updateRasterState();
	mKeyEventFilter.setListener(nullptr);
	mSettingsProvider->setActiveListener(nullptr);
	mSettingsProvider->setActiveZoomValueProvider(nullptr);
	{
		const QSignalBlocker blocker(this);
		// Include pages already removed from the tab bar but awaiting deleteLater.
		const auto pages = findChildren<AnnotationTabContent *>();
		mOpenPages.clear();
		for (auto page : pages) {
			disconnect(page->annotationArea(), nullptr, this, nullptr);
			delete page;
		}
		mItemClipboard->clear();
	}
	if (mRasterStore) {
		mRasterStore->close([this]() { emit rasterBackingClosed(); });
	} else {
		QMetaObject::invokeMethod(this, [this]() { emit rasterBackingClosed(); }, Qt::QueuedConnection);
	}
}

void AnnotationTabWidget::updateRasterState()
{
	if (!mRasterStore) { return; }
	auto ready = isCurrentImageReady();
	auto editable = ready && !mStorageFailed;
	for (auto page : mOpenPages.keys()) {
		page->setRasterReady(page == selectedPage() && ready);
		page->setRasterEditingEnabled(page == selectedPage() && editable);
	}
	auto interactive = editable ? selectedPage() : nullptr;
	if (interactive != mInteractivePage) {
		mInteractivePage = interactive;
		mKeyEventFilter.setListener(editable ? currentAnnotationArea() : nullptr);
		mSettingsProvider->setActiveListener(editable ? currentAnnotationArea() : nullptr);
		mSettingsProvider->setActiveZoomValueProvider(editable ? currentZoomValueProvider() : nullptr);
	}
	mUndoAction->setEnabled(editable && mUndoRedoEnabled);
	mRedoAction->setEnabled(editable && mUndoRedoEnabled);
	if (mLastReady != ready) {
		mLastReady = ready;
		emit currentImageReadyChanged(ready);
	}
	auto admission = canAcceptImage();
	if (mLastAdmission != admission) {
		mLastAdmission = admission;
		emit imageAdmissionChanged(admission);
	}
}

void AnnotationTabWidget::scheduleRasterWork()
{
	if (mClosing) { return; }
	if (mRasterStore) { updateRasterState(); }
	if (mPumpQueued) { return; }
	mPumpQueued = true;
	QMetaObject::invokeMethod(this, [this]() {
		mPumpQueued = false;
		runRasterWork();
	}, Qt::QueuedConnection);
}

void AnnotationTabWidget::failRasterWork(const QString &error)
{
	mStorageFailed = true;
	mPasteRequest.pending = false;
	if (mResidentPage && mOpenPages.contains(mResidentPage)) {
		mDesiredPage = mResidentPage;
		setCurrentWidget(mResidentPage);
	}
	updateRasterState();
	emit rasterBackingError(error);
}

void AnnotationTabWidget::runRasterWork()
{
	if (mClosing) { return; }
	if (mRasterStore && mRasterStore->isBusy()) { return; }
	if (mRasterStore && !mStorageFailed) {
		if (mPendingReleasePage && !mOpenPages.contains(mPendingReleasePage)) { mPendingReleasePage = nullptr; }
		if (mPendingReleasePage) {
			auto page = mPendingReleasePage;
			auto resources = page->annotationArea()->rasterDocument()->handles();
			mRasterStore->prepare(resources, false, [this, page, resources](const QString &error) {
				if (error.isEmpty() || !page || !mOpenPages.contains(page)) {
					if (page && page != mResidentPage) { page->annotationArea()->setRasterResident(false); }
					mPendingReleasePage = nullptr;
				} else {
					failRasterWork(error);
				}
				RasterDocument::releaseUnleased(resources);
				scheduleRasterWork();
			});
			updateRasterState();
			return;
		}
		auto handles = admissionHandles();
		bool needsBacking = false;
		for (const auto &handle : handles) { needsBacking |= !handle->blob; }
		if (needsBacking) {
			mRasterStore->prepare(handles, false, [this, handles](const QString &error) {
				RasterDocument::releaseUnleased(handles);
				if (!error.isEmpty()) { failRasterWork(error); }
				scheduleRasterWork();
			});
			updateRasterState();
			return;
		}
		// A close batch suppresses automatic neighbor activation, not explicit
		// exports: an asynchronous save during close must still be able to finish.
		if (!mTabBatchDepth && mDesiredPage != mResidentPage) {
			auto target = mDesiredPage;
			if (!target) {
				if (mResidentPage) {
					mResidentPage->annotationArea()->setRasterResident(false);
					mResidentPage = nullptr;
				}
				updateRasterState();
			} else {
				auto generation = mOpenPages.value(target);
				auto resources = target->annotationArea()->rasterDocument()->handles();
				target->annotationArea()->rasterDocument()->setResident(true);
				mRasterStore->prepare(resources, true, [this, target, generation, resources](const QString &error) {
					if (isOpen(target, generation) && target == mDesiredPage && error.isEmpty()) {
						if (mResidentPage && mResidentPage != target) {
							mResidentPage->annotationArea()->setRasterResident(false);
						}
						mResidentPage = target;
						target->annotationArea()->setRasterResident(true);
					} else {
						if (target) { target->annotationArea()->setRasterResident(false); }
						if (!error.isEmpty() && target == mDesiredPage) { failRasterWork(error); }
					}
					RasterDocument::releaseUnleased(resources);
					scheduleRasterWork();
				});
				updateRasterState();
				return;
			}
		}
		if (mPasteRequest.pending) {
			auto request = mPasteRequest;
			if (!isOpen(request.page, request.generation) || request.page != mResidentPage ||
				request.clipboardGeneration != mItemClipboard->generation()) {
				mPasteRequest.pending = false;
			} else {
				auto resources = mItemClipboard->rasterHandles();
				mRasterStore->prepare(resources, true, [this, request, resources](const QString &error) {
					bool valid = mPasteRequest.pending && isOpen(request.page, request.generation) &&
						request.page == mResidentPage && request.page == mDesiredPage &&
						request.clipboardGeneration == mItemClipboard->generation();
					mPasteRequest.pending = false;
					if (valid && error.isEmpty()) { request.page->annotationArea()->syncPaste(request.position); }
					RasterDocument::releaseUnleased(resources);
					if (!error.isEmpty()) { emit rasterBackingError(error); }
					scheduleRasterWork();
				});
				updateRasterState();
				return;
			}
		}
	}
	if (mRasterStore && mStorageFailed && mDesiredPage != mResidentPage) {
		if (mResidentPage) {
			mDesiredPage = mResidentPage;
			setCurrentWidget(mResidentPage);
		}
	}
	while (!mRequestQueue.isEmpty()) {
		auto id = mRequestQueue.takeFirst();
		if (!mImageRequests.contains(id)) { continue; }
		auto request = mImageRequests.value(id);
		if (!isOpen(request.page, request.generation)) {
			finishRequest(id, request.serial, {}, true, QString());
			continue;
		}
		if (mRasterStore && mStorageFailed && request.page != mResidentPage) {
			finishRequest(id, request.serial, {}, false, tr("Screenshot backing is unavailable after a cache failure."));
			continue;
		}
		if (!mRasterStore || request.page == mResidentPage) {
			auto image = request.page->annotationArea()->image();
			finishRequest(id, request.serial, image, false, image.isNull() ? tr("The screenshot is empty or not ready.") : QString());
			scheduleRasterWork();
			return;
		}
		auto resources = request.page->annotationArea()->rasterDocument()->handles();
		request.page->annotationArea()->rasterDocument()->setResident(true);
		mRasterStore->prepare(resources, true, [this, id, request, resources](const QString &error) {
			auto valid = isOpen(request.page, request.generation) && mImageRequests.contains(id) &&
				mImageRequests.value(id).serial == request.serial && !mImageRequests.value(id).canceled;
			QImage image;
			if (valid && error.isEmpty()) {
				request.page->annotationArea()->setRasterResident(true);
				image = request.page->annotationArea()->image();
			}
			auto allResources = resources;
			bool needsBacking = false;
			if (valid && error.isEmpty()) {
				auto document = request.page->annotationArea()->rasterDocument();
				allResources.append(document->handles());
				needsBacking = document->needsBacking();
			}
			auto complete = [this, id, request, image, allResources, needsBacking](const QString &error) {
				if (needsBacking && !error.isEmpty() && isOpen(request.page, request.generation)) {
					// A cold export can materialize a still-dirty obfuscation raster.
					// Keep that one bounded transient on write failure; never discard
					// its last copy or start loading another background document.
					request.page->annotationArea()->setRasterResident(false);
					failRasterWork(error);
				} else {
					if (request.page && request.page != mResidentPage) {
						request.page->annotationArea()->setRasterResident(false);
					}
					if (mPendingReleasePage == request.page) { mPendingReleasePage = nullptr; }
				}
				RasterDocument::releaseUnleased(allResources);
				finishRequest(id, request.serial, error.isEmpty() ? image : QImage(), false, !error.isEmpty() ? error :
					(image.isNull() ? tr("The screenshot is empty or not ready.") : QString()));
				scheduleRasterWork();
			};
			if (needsBacking) {
				mPendingReleasePage = request.page;
				mRasterStore->prepare(allResources, false, complete);
				updateRasterState();
			} else {
				complete(error);
			}
		});
		updateRasterState();
		return;
	}
	updateRasterState();
}

void AnnotationTabWidget::requestImageAt(int index, quint64 requestId)
{
	if (mImageRequests.contains(requestId)) {
		QMetaObject::invokeMethod(this, [this, requestId]() {
			emit imageRequestFinished(requestId, {}, false, tr("The image request ID is already pending."));
		}, Qt::QueuedConnection);
		return;
	}
	auto page = dynamic_cast<AnnotationTabContent *>(widget(index));
	auto serial = ++mNextGeneration;
	mImageRequests.insert(requestId, {page, mOpenPages.value(page), serial});
	if (!page) {
		finishRequest(requestId, serial, {}, false, tr("The requested screenshot tab does not exist."));
	} else {
		mRequestQueue.append(requestId);
		scheduleRasterWork();
	}
}

void AnnotationTabWidget::finishRequest(quint64 id, quint64 serial, const QImage &image,
									  bool canceled, const QString &error)
{
	if (!mImageRequests.contains(id) || mImageRequests.value(id).serial != serial) { return; }
	auto &request = mImageRequests[id];
	request.canceled |= canceled;
	if (request.terminalQueued) { return; }
	request.terminalQueued = true;
	mRequestQueue.removeAll(id);
	QMetaObject::invokeMethod(this, [this, id, serial, image, error]() {
		if (!mImageRequests.contains(id) || mImageRequests.value(id).serial != serial) { return; }
		auto request = mImageRequests.take(id);
		bool canceled = request.canceled || (request.generation && !isOpen(request.page, request.generation));
		emit imageRequestFinished(id, canceled ? QImage() : image, canceled, canceled ? QString() : error);
	}, Qt::QueuedConnection);
}

void AnnotationTabWidget::cancelImageRequest(quint64 requestId)
{
	if (mImageRequests.contains(requestId)) {
		finishRequest(requestId, mImageRequests.value(requestId).serial, {}, true, QString());
	}
}

void AnnotationTabWidget::requestPaste(AnnotationTabContent *page, const QPointF &position)
{
	if (!mRasterStore || page != mResidentPage || !isCurrentImageReady()) { return; }
	mStorageFailed = false; // Explicit user operation, not an automatic retry.
	mPasteRequest = {page, mOpenPages.value(page), mItemClipboard->generation(), position, true};
	scheduleRasterWork();
}

void AnnotationTabWidget::beginTabBatch()
{
	++mTabBatchDepth;
	updateRasterState();
}

void AnnotationTabWidget::endTabBatch()
{
	if (mTabBatchDepth == 0) { return; }
	--mTabBatchDepth;
	scheduleRasterWork();
}

void AnnotationTabWidget::removeTab(int index)
{
	auto page = dynamic_cast<AnnotationTabContent *>(widget(index));
	if (!page) { return; }
	mOpenPages.remove(page); // Invalidate before QTabWidget selects its neighbor.
	const auto requests = mImageRequests;
	for (auto it = requests.cbegin(); it != requests.cend(); ++it) {
		if (it.value().page == page) { cancelImageRequest(it.key()); }
	}
	if (mRasterStore) {
		if (mPasteRequest.page == page) { mPasteRequest.pending = false; }
		if (mPendingReleasePage == page) { mPendingReleasePage = nullptr; }
		if (mResidentPage == page) { mResidentPage = nullptr; }
		page->annotationArea()->setRasterResident(false);
		page->setRasterReady(false);
	}
	QTabWidget::removeTab(index);
	if (mRasterStore) { page->deleteLater(); }
	if (mRasterStore && mOpenPages.isEmpty()) { mStorageFailed = false; }
	tabChanged();
}

QImage AnnotationTabWidget::readyImageAt(int index) const
{
	auto area = annotationAreaAt(index);
	if (mRasterStore && (!area || !area->isRasterReady() || selectedPage() != widget(index) || !isCurrentImageReady())) {
		emit const_cast<AnnotationTabWidget *>(this)->rasterBackingError(
			tr("This screenshot is not ready; use requestImageAt() for an asynchronous export."));
		return {};
	}
	return area ? area->image() : QImage();
}

void AnnotationTabWidget::addContextMenuActions(const QList<QAction *> &actions)
{
	mTabContextMenu->addCustomActions(actions);
}

} // namespace kImageAnnotator
