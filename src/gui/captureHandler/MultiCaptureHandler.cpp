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

#include "MultiCaptureHandler.h"

MultiCaptureHandler::MultiCaptureHandler(
		IImageAnnotator *imageAnnotator,
		const QSharedPointer<INotificationService> &notificationService,
		const QSharedPointer<ICaptureTabStateHandler> &captureTabStateHandler,
		const QSharedPointer<IConfig> &config,
		const QSharedPointer<IClipboard> &clipboard,
		const QSharedPointer<IDesktopService> &desktopService,
		const QSharedPointer<IFileService> &fileService,
		const QSharedPointer<IMessageBoxService> &messageBoxService,
		const QSharedPointer<IRecentImageService> &recentImageService,
		const QSharedPointer<IImageSaver> &imageSaver,
		const QSharedPointer<ISavePathProvider> &savePathProvider,
		const QSharedPointer<IIconLoader> &iconLoader,
		const QSharedPointer<IFileDialogService> &fileDialogService,
		QWidget *parent) :
	mImageAnnotator(imageAnnotator),
	mNotificationService(notificationService),
	mParent(parent),
	mCaptureChangeListener(nullptr),
	mTabStateHandler(captureTabStateHandler),
	mConfig(config),
	mClipboard(clipboard),
	mDesktopService(desktopService),
	mMessageBoxService(messageBoxService),
	mFileService(fileService),
	mRecentImageService(recentImageService),
	mImageSaver(imageSaver),
	mSavePathProvider(savePathProvider),
	mFileDialogService(fileDialogService),
	mSaveContextMenuAction(new TabContextMenuAction(this)),
	mSaveAsContextMenuAction(new TabContextMenuAction(this)),
	mSaveAllContextMenuAction(new TabContextMenuAction(this)),
	mRenameContextMenuAction(new TabContextMenuAction(this)),
	mOpenDirectoryContextMenuAction(new TabContextMenuAction(this)),
	mCopyPathToClipboardContextMenuAction(new TabContextMenuAction(this)),
	mCopyToClipboardContextMenuAction(new TabContextMenuAction(this)),
	mDeleteImageContextMenuAction(new TabContextMenuAction(this)),
	mContextMenuSeparatorAction(new QAction(this))
{
	connect(mImageAnnotator, &IImageAnnotator::currentTabChanged, mTabStateHandler.data(), &ICaptureTabStateHandler::currentTabChanged);
	connect(mImageAnnotator, &IImageAnnotator::tabMoved, mTabStateHandler.data(), &ICaptureTabStateHandler::tabMoved);
	// imageChanged is a content-edit hook, including undo/redo, effects and canvas changes.
	// Hydration/export must only report readiness, never an edit to the selected document.
	connect(mImageAnnotator, &IImageAnnotator::imageChanged, mTabStateHandler.data(), &ICaptureTabStateHandler::currentTabContentChanged);
	connect(mTabStateHandler.data(), &ICaptureTabStateHandler::updateTabInfo, mImageAnnotator, &IImageAnnotator::updateTabInfo);

	connect(mImageAnnotator, &IImageAnnotator::imageChanged, this, &MultiCaptureHandler::captureChanged);
	connect(mImageAnnotator, &IImageAnnotator::currentTabChanged, this, &MultiCaptureHandler::captureChanged);
	connect(mImageAnnotator, &IImageAnnotator::tabCloseRequested, this, &MultiCaptureHandler::tabCloseRequested);
	connect(mImageAnnotator, &IImageAnnotator::tabCloseBatchRequested, this, &MultiCaptureHandler::tabCloseBatchRequested);
	connect(mImageAnnotator, &IImageAnnotator::imageRequestFinished, this, &MultiCaptureHandler::imageRequestFinished);
	connect(mImageAnnotator, &IImageAnnotator::currentImageReadyChanged, this, &MultiCaptureHandler::captureChanged);
	connect(mImageAnnotator, &QObject::destroyed, this, &MultiCaptureHandler::cancelPendingOperations);
	connect(mImageAnnotator, &IImageAnnotator::tabContextMenuOpened, this, &MultiCaptureHandler::updateContextMenuActions);

	connect(mConfig.data(), &IConfig::annotatorConfigChanged, this, &MultiCaptureHandler::annotatorConfigChanged);

	addTabContextMenuActions(iconLoader);

	annotatorConfigChanged();
}

MultiCaptureHandler::~MultiCaptureHandler()
{
	mCaptureChangeListener = nullptr;
	cancelPendingOperations();
}

bool MultiCaptureHandler::canClose()
{
	// The legacy bool API cannot wait for a cold page. Resident callers use requestClose().
	if (!mImageAnnotator || isBusy() || (mImageAnnotator->rasterBackingEnabled() && mTabStateHandler->count() != 0)) {
		return false;
	}
	QPointer<MultiCaptureHandler> guard(this);
	while (mTabStateHandler->count() != 0) {
		int index = mTabStateHandler->currentTabIndex();
		auto document = mTabStateHandler->documentId(index);
		if (!discardChanges(index) || !guard) {
			return false;
		}
		if (document != 0) {
			index = mTabStateHandler->indexForDocument(document);
			if (index < 0) {
				return false;
			}
		}
		removeTab(index);
		if (!guard) {
			return false;
		}
	}
	return true;
}

void MultiCaptureHandler::requestClose(const std::function<void(bool)> &finished)
{
	if (!mImageAnnotator || isBusy()) {
		finished(false);
		return;
	}
	if (!mImageAnnotator->rasterBackingEnabled()) {
		finished(canClose());
		return;
	}
	QList<quint64> documents;
	auto current = mTabStateHandler->currentTabIndex();
	auto currentDocument = mTabStateHandler->documentId(current);
	if (currentDocument != 0) {
		documents.append(currentDocument);
	}
	for (auto index = mTabStateHandler->count() - 1; index >= 0; --index) {
		if (index != current) {
			documents.append(mTabStateHandler->documentId(index));
		}
	}
	startCommand(Command::Quit, documents, true, true, finished);
}

bool MultiCaptureHandler::isBusy() const
{
	return mPending.command != Command::None;
}

void MultiCaptureHandler::cancelPendingOperations()
{
	if (!isBusy()) {
		return;
	}
	auto request = mPending.request;
	auto serial = mPending.serial;
	mPending.request = 0;
	QPointer<MultiCaptureHandler> guard(this);
	if (request != 0 && mImageAnnotator) {
		mImageAnnotator->cancelImageRequest(request);
	}
	if (guard && mPending.serial == serial) {
		finishCommand(false);
	}
}

bool MultiCaptureHandler::canTakeNew()
{
	return !isBusy();
}

bool MultiCaptureHandler::discardChanges(int index)
{
	QPointer<MultiCaptureHandler> guard(this);
	auto document = mTabStateHandler->documentId(index);
	auto revision = document == 0 ? 0 : mTabStateHandler->revision(index);
	auto response = CanDiscardOperation::confirmDiscard(!mTabStateHandler->isSaved(index),
			mTabStateHandler->filename(index), mMessageBoxService, mConfig);
	if (!guard) {
		return false;
	}
	if (document != 0) {
		index = mTabStateHandler->indexForDocument(document);
		if (index < 0 || mTabStateHandler->revision(index) != revision) {
			return false;
		}
	}
	if (response == MessageBoxResponse::Yes) {
		return saveAt(index, true, true);
	}
	return response != MessageBoxResponse::Cancel;
}

void MultiCaptureHandler::removeTab(int currentTabIndex)
{
	QPointer<MultiCaptureHandler> guard(this);
	mImageAnnotator->removeTab(currentTabIndex);
	if (!guard || !mImageAnnotator) {
		return;
	}
	mTabStateHandler->tabRemoved(currentTabIndex);
	captureChanged();
	if (!guard || !mImageAnnotator) {
		return;
	}

	if(mTabStateHandler->count() == 0) {
		mImageAnnotator->hide();
		if (guard) {
			captureEmpty();
		}
	}
}

bool MultiCaptureHandler::isSaved() const
{
	return mTabStateHandler->isSaved(mTabStateHandler->currentTabIndex());
}

QString MultiCaptureHandler::path() const
{
	return mTabStateHandler->path(mTabStateHandler->currentTabIndex());
}

bool MultiCaptureHandler::isPathValid() const
{
	return mTabStateHandler->isPathValid(mTabStateHandler->currentTabIndex());
}

void MultiCaptureHandler::saveAs()
{
	saveAsTab(mTabStateHandler->currentTabIndex());
}

void MultiCaptureHandler::save(bool showSuccessToast)
{
	saveAt(mTabStateHandler->currentTabIndex(), true, showSuccessToast);
}

void MultiCaptureHandler::saveAll()
{
	saveAllTabs();
}

void MultiCaptureHandler::rename()
{
	renameTab(mTabStateHandler->currentTabIndex());
}

void MultiCaptureHandler::copy()
{
	copyToClipboardTab(mTabStateHandler->currentTabIndex());
}

void MultiCaptureHandler::copyPath()
{
	copyPathToClipboardTab(mTabStateHandler->currentTabIndex());
}

void MultiCaptureHandler::openDirectory()
{
	openDirectoryTab(mTabStateHandler->currentTabIndex());
}

void MultiCaptureHandler::removeImage()
{
	deleteImageTab(mTabStateHandler->currentTabIndex());
}

bool MultiCaptureHandler::saveAt(int index, bool isInstant, bool showSuccessToast)
{
	if (isBusy()) {
		return false;
	}
	if (mImageAnnotator->rasterBackingEnabled()) {
		return startCommand(Command::Save, {mTabStateHandler->documentId(index)}, isInstant, showSuccessToast);
	}
	return saveImageAt(index, mImageAnnotator->imageAt(index), isInstant, showSuccessToast);
}

bool MultiCaptureHandler::saveImageAt(int index, const QImage &image, bool isInstant, bool showSuccessToast)
{
	QPointer<MultiCaptureHandler> guard(this);
	auto document = mTabStateHandler->documentId(index);
	auto revision = document == 0 ? 0 : mTabStateHandler->revision(index);
	SaveOperation operation(
			image,
			isInstant,
			mTabStateHandler->path(index),
			mNotificationService,
			mRecentImageService,
			mImageSaver,
			mSavePathProvider,
			mFileDialogService,
			mConfig,
			mParent);
	auto saveResult = operation.execute(showSuccessToast);
	if (!guard) {
		return false;
	}
	auto successful = saveResult.isSuccessful;
	if (document == 0) {
		// Legacy external state handlers and mocks may not provide document identities.
		mTabStateHandler->setSaveState(index, saveResult);
	} else {
		successful = mTabStateHandler->setSaveStateIfRevision(document, revision, saveResult);
	}
	if (!guard) {
		return false;
	}
	captureChanged();
	if (!guard) {
		return false;
	}
	if (document != 0 && successful) {
		index = mTabStateHandler->indexForDocument(document);
		return index >= 0 && mTabStateHandler->revision(index) == revision && mTabStateHandler->isSaved(index);
	}
	return successful;
}

void MultiCaptureHandler::load(const CaptureDto &capture)
{
	if (isBusy()) {
		return;
	}
	auto path = mPathFromCaptureProvider.pathFrom(capture);
	auto isSaved = PathHelper::isPathValid(path);
	auto filename = mNewCaptureNameProvider.nextName(path);
	auto index = mImageAnnotator->addTab(capture.screenshot, filename, path);
	if (index < 0) {
		return;
	}
	mTabStateHandler->add(index, filename, path, isSaved);
	if (capture.isCursorValid()) {
		mImageAnnotator->insertImageItem(capture.cursor.position, capture.cursor.image);
	}
}

void MultiCaptureHandler::tabCloseRequested(int index)
{
	if (isBusy()) {
		return;
	}
	if (mImageAnnotator->rasterBackingEnabled()) {
		tabCloseBatchRequested({index});
		return;
	}
	QPointer<MultiCaptureHandler> guard(this);
	auto document = mTabStateHandler->documentId(index);
	if (!discardChanges(index) || !guard) {
		return;
	}
	if (document != 0) {
		index = mTabStateHandler->indexForDocument(document);
		if (index < 0) {
			return;
		}
	}
	removeTab(index);
}

QImage MultiCaptureHandler::image() const
{
	if (!mImageAnnotator->isCurrentImageReady()) {
		return {};
	}
	return mImageAnnotator->image();
}

void MultiCaptureHandler::insertImageItem(const QPointF &pos, const QPixmap &pixmap)
{
	if (isBusy() || !mImageAnnotator->isCurrentImageReady()) {
		return;
	}
	mImageAnnotator->insertImageItem(pos, pixmap);
}

void MultiCaptureHandler::addListener(ICaptureChangeListener *captureChangeListener)
{
	mCaptureChangeListener = captureChangeListener;
}

void MultiCaptureHandler::captureChanged()
{
	if(mCaptureChangeListener != nullptr) {
		mCaptureChangeListener->captureChanged();
	}
}

void MultiCaptureHandler::captureEmpty()
{
	if(mCaptureChangeListener != nullptr) {
		mCaptureChangeListener->captureEmpty();
	}
}

void MultiCaptureHandler::annotatorConfigChanged()
{
	mImageAnnotator->setTabBarAutoHide(mConfig->autoHideTabs());
}

void MultiCaptureHandler::addTabContextMenuActions(const QSharedPointer<IIconLoader> &iconLoader)
{
	mSaveContextMenuAction->setText(tr("Save"));
	mSaveContextMenuAction->setIcon(iconLoader->loadForTheme(QLatin1String("save.svg")));

	mSaveAsContextMenuAction->setText(tr("Save As"));
	mSaveAsContextMenuAction->setIcon(iconLoader->loadForTheme(QLatin1String("saveAs.svg")));

	mSaveAllContextMenuAction->setText(tr("Save All"));
	mSaveAllContextMenuAction->setIcon(iconLoader->loadForTheme(QLatin1String("save.svg")));

	mRenameContextMenuAction->setText(tr("Rename"));

	mOpenDirectoryContextMenuAction->setText(tr("Open Directory"));

	mCopyToClipboardContextMenuAction->setText(tr("Copy"));
	mCopyToClipboardContextMenuAction->setIcon(iconLoader->loadForTheme(QLatin1String("copy.svg")));

	mCopyPathToClipboardContextMenuAction->setText(tr("Copy Path"));

	mContextMenuSeparatorAction->setSeparator(true);

	mDeleteImageContextMenuAction->setText(tr("Delete"));
	mDeleteImageContextMenuAction->setIcon(iconLoader->loadForTheme(QLatin1String("delete.svg")));

	connect(mSaveContextMenuAction, &TabContextMenuAction::triggered, this, &MultiCaptureHandler::saveTab);
	connect(mSaveAsContextMenuAction, &TabContextMenuAction::triggered, this, &MultiCaptureHandler::saveAsTab);
	connect(mSaveAllContextMenuAction, &TabContextMenuAction::triggered, this, &MultiCaptureHandler::saveAllTabs);
	connect(mRenameContextMenuAction, &TabContextMenuAction::triggered, this, &MultiCaptureHandler::renameTab);
	connect(mOpenDirectoryContextMenuAction, &TabContextMenuAction::triggered, this, &MultiCaptureHandler::openDirectoryTab);
	connect(mCopyPathToClipboardContextMenuAction, &TabContextMenuAction::triggered, this, &MultiCaptureHandler::copyPathToClipboardTab);
	connect(mCopyToClipboardContextMenuAction, &TabContextMenuAction::triggered, this, &MultiCaptureHandler::copyToClipboardTab);
	connect(mDeleteImageContextMenuAction, &TabContextMenuAction::triggered, this, &MultiCaptureHandler::deleteImageTab);

	auto actions = QList<QAction *>{mSaveContextMenuAction,
									mSaveAsContextMenuAction,
									mSaveAllContextMenuAction,
									mRenameContextMenuAction,
									mOpenDirectoryContextMenuAction,
									mCopyToClipboardContextMenuAction,
									mCopyPathToClipboardContextMenuAction,
									mContextMenuSeparatorAction,
									mDeleteImageContextMenuAction
	};
	mImageAnnotator->addTabContextMenuActions(actions);
}

void MultiCaptureHandler::saveAsTab(int index)
{
	saveAt(index, false, true);
}

void MultiCaptureHandler::saveTab(int index)
{
	saveAt(index, true, true);
}

void MultiCaptureHandler::saveAllTabs()
{
	if (isBusy()) {
		return;
	}
	if (mImageAnnotator->rasterBackingEnabled()) {
		QList<quint64> documents;
		for (auto index = 0; index < mTabStateHandler->count(); ++index) {
			if (!mTabStateHandler->isSaved(index)) {
				documents.append(mTabStateHandler->documentId(index));
			}
		}
		startCommand(Command::SaveAll, documents);
		return;
	}
	int tabIndex = 0;
	while (tabIndex < mTabStateHandler->count()) {
		if (!mTabStateHandler->isSaved(tabIndex)) {
			saveAt(tabIndex, true, true);
		}
		++tabIndex;
	}
}

void MultiCaptureHandler::renameTab(int index)
{
	if (isBusy()) {
		return;
	}
	QPointer<MultiCaptureHandler> guard(this);
	RenameOperation operation(mTabStateHandler->path(index), mTabStateHandler->filename(index), mNotificationService, mConfig, mParent);
	auto renameResult = operation.execute();
	if (guard && renameResult.isSuccessful) {
		mTabStateHandler->renameFile(index, renameResult);
		captureChanged();
	}
}

void MultiCaptureHandler::updateContextMenuActions(int index)
{
	auto isPathValid = mTabStateHandler->isPathValid(index);
	mSaveContextMenuAction->setEnabled(!isBusy() && !mTabStateHandler->isSaved(index));
	mSaveAsContextMenuAction->setEnabled(!isBusy());
	mSaveAllContextMenuAction->setEnabled(!isBusy());
	mCopyToClipboardContextMenuAction->setEnabled(!isBusy());
	mRenameContextMenuAction->setEnabled(!isBusy() && isPathValid);
	mOpenDirectoryContextMenuAction->setEnabled(isPathValid);
	mCopyPathToClipboardContextMenuAction->setEnabled(isPathValid);
	mDeleteImageContextMenuAction->setEnabled(!isBusy() && isPathValid);
}

void MultiCaptureHandler::openDirectoryTab(int index)
{
	auto path = mTabStateHandler->path(index);
	mDesktopService->openFile(PathHelper::extractParentDirectory(path));
}

void MultiCaptureHandler::copyToClipboardTab(int index)
{
	if (isBusy()) {
		return;
	}
	if (mImageAnnotator->rasterBackingEnabled()) {
		startCommand(Command::Copy, {mTabStateHandler->documentId(index)});
		return;
	}
	auto image = mImageAnnotator->imageAt(index);
	mClipboard->setImage(image);
}

void MultiCaptureHandler::copyPathToClipboardTab(int index)
{
	auto path = mTabStateHandler->path(index);
	mClipboard->setText(path);
}

void MultiCaptureHandler::deleteImageTab(int index)
{
	if (isBusy()) {
		return;
	}
	QPointer<MultiCaptureHandler> guard(this);
	auto path = mTabStateHandler->path(index);

	DeleteImageOperation operation(path, mFileService.data(), mMessageBoxService.data());

	if (operation.execute() && guard) {
		removeTab(index);
	}
}

void MultiCaptureHandler::tabCloseBatchRequested(const QList<int> &indices)
{
	if (isBusy()) {
		return;
	}
	auto orderedIndices = indices;
	std::sort(orderedIndices.begin(), orderedIndices.end(), std::greater<int>());
	QList<quint64> documents;
	for (auto index : orderedIndices) {
		auto document = mTabStateHandler->documentId(index);
		if (document != 0 && !documents.contains(document)) {
			documents.append(document);
		}
	}
	startCommand(Command::CloseTabs, documents);
}

bool MultiCaptureHandler::startCommand(Command command, const QList<quint64> &documents, bool isInstant,
		bool showSuccessToast, const std::function<void(bool)> &finished)
{
	if (isBusy() || !mImageAnnotator) {
		if (finished) {
			finished(false);
		}
		return false;
	}
	mPending.command = command;
	mPending.documents = documents;
	mPending.serial = mNextCommandId++;
	mPending.isInstant = isInstant;
	mPending.showSuccessToast = showSuccessToast;
	mPending.finished = finished;
	mPending.tabBatch = command == Command::CloseTabs || command == Command::Quit;
	auto serial = mPending.serial;
	QPointer<MultiCaptureHandler> guard(this);
	if (mPending.tabBatch) {
		mImageAnnotator->beginTabBatch();
	}
	if (!guard || mPending.serial != serial) {
		return false;
	}
	captureChanged();
	if (!guard || mPending.serial != serial) {
		return false;
	}
	processPending();
	return true;
}

void MultiCaptureHandler::processPending()
{
	QPointer<MultiCaptureHandler> guard(this);
	auto serial = mPending.serial;
	while (isBusy() && mPending.request == 0 && mPending.next < mPending.documents.count()) {
		mPending.document = mPending.documents.at(mPending.next++);
		auto index = mTabStateHandler->indexForDocument(mPending.document);
		if (index < 0) {
			if (!completeEntry(false)) {
				return;
			}
			continue;
		}
		mPending.revision = mTabStateHandler->revision(index);
		if (mPending.command == Command::CloseTabs || mPending.command == Command::Quit) {
			auto response = CanDiscardOperation::confirmDiscard(!mTabStateHandler->isSaved(index),
					mTabStateHandler->filename(index), mMessageBoxService, mConfig);
			if (!guard || mPending.serial != serial) {
				return;
			}
			if (response == MessageBoxResponse::Cancel || !pendingDocumentIsCurrentRevision()) {
				if (!completeEntry(false)) {
					return;
				}
				continue;
			}
			if (response == MessageBoxResponse::No) {
				removeTab(mTabStateHandler->indexForDocument(mPending.document));
				if (!guard || mPending.serial != serial) {
					return;
				}
				continue;
			}
		}
		index = mTabStateHandler->indexForDocument(mPending.document);
		if (!mImageAnnotator->rasterBackingEnabled() ||
				(index == mTabStateHandler->currentTabIndex() && mImageAnnotator->isCurrentImageReady())) {
			auto image = mImageAnnotator->rasterBackingEnabled() ? mImageAnnotator->image() : mImageAnnotator->imageAt(index);
			if (!guard || mPending.serial != serial) {
				return;
			}
			auto successful = completeImageCommand(image);
			if (!guard || mPending.serial != serial || !completeEntry(successful)) {
				return;
			}
		} else {
			mPending.request = mNextRequestId++;
			mImageAnnotator->requestImageAt(index, mPending.request);
			return;
		}
	}
	if (isBusy() && mPending.request == 0) {
		finishCommand(mPending.successful);
	}
}

bool MultiCaptureHandler::pendingDocumentIsCurrentRevision() const
{
	auto index = mTabStateHandler->indexForDocument(mPending.document);
	return index >= 0 && mTabStateHandler->revision(index) == mPending.revision;
}

bool MultiCaptureHandler::completeImageCommand(const QImage &image)
{
	if (image.isNull()) {
		reportImageError(tr("The capture image is unavailable."));
		return false;
	}
	if (!pendingDocumentIsCurrentRevision()) {
		return false;
	}
	QPointer<MultiCaptureHandler> guard(this);
	auto serial = mPending.serial;
	if (mPending.command == Command::Copy) {
		mClipboard->setImage(image);
		return true;
	}
	auto index = mTabStateHandler->indexForDocument(mPending.document);
	SaveOperation operation(image, mPending.isInstant, mTabStateHandler->path(index), mNotificationService,
			mRecentImageService, mImageSaver, mSavePathProvider, mFileDialogService, mConfig, mParent);
	auto result = operation.execute(mPending.showSuccessToast);
	if (!guard || mPending.serial != serial) {
		return false;
	}
	// A modal save dialog may dispatch edits, moves, cancellation or destruction.
	auto savedCurrentRevision = mTabStateHandler->setSaveStateIfRevision(mPending.document, mPending.revision, result);
	if (!guard || mPending.serial != serial) {
		return false;
	}
	if (savedCurrentRevision && (mPending.command == Command::CloseTabs || mPending.command == Command::Quit)) {
		removeTab(mTabStateHandler->indexForDocument(mPending.document));
	} else {
		captureChanged();
	}
	return savedCurrentRevision;
}

bool MultiCaptureHandler::completeEntry(bool successful)
{
	if (!successful) {
		mPending.successful = false;
		if (mPending.command == Command::Quit) {
			finishCommand(false);
			return false;
		}
	}
	return true;
}

void MultiCaptureHandler::imageRequestFinished(quint64 requestId, const QImage &image, bool canceled, const QString &error)
{
	if (!isBusy() || mPending.request == 0 || requestId != mPending.request) {
		return;
	}
	mPending.request = 0;
	auto serial = mPending.serial;
	QPointer<MultiCaptureHandler> guard(this);
	auto successful = !canceled && error.isEmpty() && !image.isNull();
	if (!successful && !canceled) {
		reportImageError(error.isEmpty() ? tr("The capture image is unavailable.") : error);
	}
	if (!guard || mPending.serial != serial) {
		return;
	}
	if (successful) {
		successful = completeImageCommand(image);
	}
	if (!guard || mPending.serial != serial) {
		return;
	}
	if (completeEntry(successful)) {
		processPending();
	}
}

void MultiCaptureHandler::finishCommand(bool successful)
{
	auto finished = std::move(mPending.finished);
	auto serial = mPending.serial;
	auto tabBatch = mPending.tabBatch;
	mPending.tabBatch = false;
	QPointer<MultiCaptureHandler> guard(this);
	if (tabBatch && mImageAnnotator) {
		mImageAnnotator->endTabBatch();
	}
	if (!guard || mPending.serial != serial) {
		if (finished) {
			finished(false);
		}
		return;
	}
	mPending = PendingCommand();
	if (finished) {
		finished(successful);
	}
	if (guard) {
		captureChanged();
	}
}

void MultiCaptureHandler::reportImageError(const QString &error)
{
	if (mNotificationService) {
		NotifyOperation operation(tr("Loading Capture Failed"), error, NotificationTypes::Critical, mNotificationService, mConfig);
		operation.execute();
	} else {
		qWarning("Loading Capture Failed: %s", qPrintable(error));
	}
}
