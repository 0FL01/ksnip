/*
 *  Copyright (C) 2016 Damir Porobic <damir.porobic@gmx.com>
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
 *
 */

#include "MainWindow.h"

MainWindow::MainWindow(DependencyInjector *dependencyInjector) :
	QMainWindow(),
	mDependencyInjector(dependencyInjector),
	mConfig(mDependencyInjector->get<IConfig>()),
	mImageGrabber(mDependencyInjector->get<IImageGrabber>()),
	mPluginManager(mDependencyInjector->get<IPluginManager>()),
	mTrayIcon(new TrayIcon(mConfig, mDependencyInjector->get<IIconLoader>(), this)),
	mNotificationService(NotificationServiceFactory::create(mTrayIcon, mDependencyInjector->get<IPlatformChecker>(), mConfig)),
	mToolBar(nullptr),
	mImageAnnotator(new KImageAnnotatorAdapter),
	mSaveAsAction(new QAction(this)),
	mSaveAllAction(new QAction(this)),
	mUploadAction(new QAction(this)),
	mCopyAsDataUriAction(new QAction(this)),
	mPrintAction(new QAction(this)),
	mPrintPreviewAction(new QAction(this)),
	mQuitAction(new QAction(this)),
	mCopyPathAction(new QAction(this)),
	mRenameAction(new QAction(this)),
	mOpenDirectoryAction(new QAction(this)),
	mToggleDocksAction(new QAction(this)),
	mSettingsAction(new QAction(this)),
	mConfigureGlobalShortcutsAction(new QAction(this)),
	mAboutAction(new QAction(this)),
	mOpenImageAction(new QAction(this)),
	mScaleAction(new QAction(this)),
	mRotateAction(new QAction(this)),
	mAddWatermarkAction(new QAction(this)),
	mPasteAction(new QAction(this)),
	mPasteEmbeddedAction(new QAction(this)),
	mPinAction(new QAction(this)),
	mRemoveImageAction(new QAction(this)),
	mModifyCanvasAction(new QAction(this)),
	mCloseWindowAction(new QAction(this)),
	mOcrAction(new QAction(this)),
	mCutAction(new QAction(this)),
	mMainLayout(layout()),
	mActionsMenu(new ActionsMenu(mConfig)),
	mRecentImagesMenu(new RecentImagesMenu(mDependencyInjector->get<IRecentImageService>(), this)),
	mClipboard(mDependencyInjector->get<IClipboard>()),
	mCapturePrinter(new CapturePrinter(this)),
	mGlobalHotKeyHandler(new GlobalHotKeyHandler(mImageGrabber->supportedCaptureModes(), mDependencyInjector->get<IPlatformChecker>(), mConfig, this)),
	mDragAndDropProcessor(new DragAndDropProcessor(this, mDependencyInjector->get<ITempFileProvider>(), [this] { return canAcquireImage(); })),
	mUploadHandler(mDependencyInjector->get<IUploadHandler>()),
	mSessionManagerRequestedQuit(false),
	mResizeOnNormalize(false),
	mHideEditorAfterCaptureEnabled(mConfig->hideEditorAfterCaptureEnabled()),
	mCapturePending(false),
	mCaptureHandler(CaptureHandlerFactory::create(mImageAnnotator, mNotificationService, mDependencyInjector, this)),
	mPinWindowHandler(mDependencyInjector->get<IPinWindowHandler>()),
	mVisibilityHandler(WidgetVisibilityHandlerFactory::create(this, mDependencyInjector->get<IPlatformChecker>())),
	mFileDialogService(mDependencyInjector->get<IFileDialogService>()),
	mWindowResizer(new WindowResizer(this, mConfig, this)),
	mActionProcessor(new ActionProcessor),
	mSavePathProvider(mDependencyInjector->get<ISavePathProvider>()),
	mOcrWindowHandler(mDependencyInjector->get<IOcrWindowHandler>()),
	mDelayHandler(mDependencyInjector->get<IDelayHandler>())
#ifdef UNIX_X11
	,mOcrCaptureWorkflow(new OcrCaptureWorkflow(mClipboard, this))
#ifdef KSNIP_BUILTIN_OCR
	,mOcrRecognizer(new PaddleOcrRecognizer)
#endif
#endif
{
	initGui();

	setPosition();

	auto coreApplication = dynamic_cast<QApplication *>(QCoreApplication::instance());
	connect(coreApplication, &QGuiApplication::commitDataRequest, this, &MainWindow::sessionFinished);

	setAcceptDrops(true);
	coreApplication->installEventFilter(mDragAndDropProcessor);
	connect(mDragAndDropProcessor, &DragAndDropProcessor::fileDropped, this, &MainWindow::loadImageFromFile);
	connect(mDragAndDropProcessor, &DragAndDropProcessor::imageDropped, this, &MainWindow::loadImageFromPixmap);
	connect(mDragAndDropProcessor, &DragAndDropProcessor::temporaryFileDropped, this, &MainWindow::loadTemporaryImageFile);
	connect(mDragAndDropProcessor, &DragAndDropProcessor::imageInputRejected, this, &MainWindow::notifyImageInputBusy);
	connect(mImageAnnotator, &IImageAnnotator::currentImageReadyChanged, this, [this] {
		refreshImageEnablements();
		scheduleInputPump();
	});
	connect(mImageAnnotator, &IImageAnnotator::imageAdmissionChanged, this, [this] {
		refreshImageEnablements();
		scheduleInputPump();
	});
	connect(mImageAnnotator, &IImageAnnotator::rasterBackingError, this, [this](const QString &message) {
		NotifyOperation operation(tr("Screenshot backing storage"), message, NotificationTypes::Critical, mNotificationService, mConfig);
		operation.execute();
		refreshImageEnablements();
	});

	connect(mConfig.data(), &IConfig::annotatorConfigChanged, this, &MainWindow::setupImageAnnotator);

#ifdef UNIX_X11
	connect(mImageGrabber.data(), &IImageGrabber::finished, mOcrCaptureWorkflow, &OcrCaptureWorkflow::processCapture);
	connect(mImageGrabber.data(), &IImageGrabber::canceled, mOcrCaptureWorkflow, &OcrCaptureWorkflow::processCancellation);
	connect(mOcrCaptureWorkflow, &OcrCaptureWorkflow::editorCaptureReady, this, [this](const CaptureDto &capture) {
		mCapturePending = false;
		processScreenshotCapture(capture);
	});
	connect(mOcrCaptureWorkflow, &OcrCaptureWorkflow::editorCaptureCanceled, this, [this] {
		mCapturePending = false;
		captureCanceled();
	});
	connect(mOcrCaptureWorkflow, &OcrCaptureWorkflow::editorCaptureCanceled, mActionProcessor, &ActionProcessor::captureCanceled);
	connect(mOcrCaptureWorkflow, &OcrCaptureWorkflow::captureReleased, this, [this] {
		mCapturePending = false;
		releaseImageInput();
		mVisibilityHandler->restoreState();
	});
	connect(mOcrCaptureWorkflow, &OcrCaptureWorkflow::captureRequested, this, [this] {
		auto isMainWindowVisible = !isMinimized();
		auto adjustedDelay = mDelayHandler->getDelay(0, isMainWindowVisible);
		hideMainWindowIfRequired();
		mCapturePending = true;
		mImageGrabber->grabImage(CaptureModes::RectArea, false, adjustedDelay);
	});
#else
	connect(mImageGrabber.data(), &IImageGrabber::finished, this, [this](const CaptureDto &capture) {
		mCapturePending = false;
		processScreenshotCapture(capture);
	});
	connect(mImageGrabber.data(), &IImageGrabber::canceled, this, [this] {
		mCapturePending = false;
		captureCanceled();
	});
	connect(mImageGrabber.data(), &IImageGrabber::canceled, mActionProcessor, &ActionProcessor::captureCanceled);
#endif

	connect(mGlobalHotKeyHandler, &GlobalHotKeyHandler::captureTriggered, this, &MainWindow::triggerCapture);
	connect(mGlobalHotKeyHandler, &GlobalHotKeyHandler::actionTriggered, this, &MainWindow::actionTriggered);
#ifdef KSNIP_BUILTIN_OCR
	connect(mGlobalHotKeyHandler, &GlobalHotKeyHandler::ocrRectAreaTriggered, this, &MainWindow::triggerOcrCapture);
#endif

	connect(mUploadHandler.data(), &IUploadHandler::finished, this, &MainWindow::uploadFinished);

	connect(mRecentImagesMenu, &RecentImagesMenu::openRecentSelected, this, &MainWindow::loadImageFromFile);

	connect(this, &MainWindow::imageLoaded, mActionProcessor, &ActionProcessor::captureFinished);
	connect(mActionProcessor, &ActionProcessor::triggerCapture, this, &MainWindow::capture);
	connect(mActionProcessor, &ActionProcessor::triggerPinImage, mPinAction, &QAction::trigger);
	connect(mActionProcessor, &ActionProcessor::triggerUpload, mUploadAction, &QAction::trigger);
	connect(mActionProcessor, &ActionProcessor::triggerOpenDirectory, mCaptureHandler, &ICaptureHandler::openDirectory);
	connect(mActionProcessor, &ActionProcessor::triggerCopyToClipboard, mToolBar->copyToClipboardAction(), &QAction::trigger);
	connect(mActionProcessor, &ActionProcessor::triggerSave, mToolBar->saveAction(), &QAction::trigger);
	connect(mActionProcessor, &ActionProcessor::triggerShow, this, &MainWindow::showAfterAction);

	mCaptureHandler->addListener(this);

	setupImageAnnotator();
	mImageAnnotator->enableRasterBacking();
	handleGuiStartup();
	resize(minimumSize());
	loadSettings();
}

MainWindow::~MainWindow()
{
	mCaptureHandler->cancelPendingOperations();
	delete mCaptureHandler;
	delete mImageAnnotator;
	delete mCapturePrinter;
	delete mDragAndDropProcessor;
	delete mVisibilityHandler;
	delete mWindowResizer;
	delete mActionProcessor;
	delete mActionsMenu;
}

void MainWindow::handleGuiStartup()
{
	if (mConfig->captureOnStartup()) {
		triggerCapture(mConfig->captureMode());
	} else if (mTrayIcon->isVisible() && mConfig->startMinimizedToTray()) {
		showHidden();
	} else {
		showEmpty();
	}
}

void MainWindow::setPosition()
{
	auto position = mConfig->windowPosition();
	auto desktopGeometry = QApplication::primaryScreen()->geometry();

	if(!desktopGeometry.contains(position)) {
		auto screenCenter = desktopGeometry.center();
		auto mainWindowSize = size();
		position = QPoint(screenCenter.x() - mainWindowSize.width() / 2, screenCenter.y() - mainWindowSize.height() / 2);
	}
	move(position);
}

void MainWindow::captureScreenshot(CaptureModes captureMode, bool captureCursor, int delay)
{
	// Reject a second producer without canceling the action owning the first
	// capture (or a replacement-confirmation dialog's reservation).
	if (mCapturePending || mImageInputReserved) {
		return;
	}
#ifdef UNIX_X11
	if (mOcrCaptureWorkflow->isCapturePending()) {
		return;
	}
#endif
	if (!reserveImageInput(true)) {
		mActionProcessor->captureCanceled();
		return;
	}
	hideMainWindowIfRequired();
	mCapturePending = true;
	mImageGrabber->grabImage(captureMode, captureCursor, delay);
}

#ifdef KSNIP_BUILTIN_OCR
void MainWindow::triggerOcrCapture()
{
	if(!reserveImageInput(true)) {
		return;
	}
	if (!mOcrCaptureWorkflow->start(mOcrRecognizer)) {
		releaseImageInput();
	}
}
#endif

void MainWindow::quit()
{
	if (mQuitPending) {
		return;
	}
	if (mImageInputReserved || mCapturePending) {
		mQuitAfterInput = true;
		return;
	}
	mQuitPending = true;
	refreshImageEnablements();
	const QPointer<MainWindow> window(this);
	mCaptureHandler->requestClose([window](bool allowed) {
		if (!window) {
			return;
		}
		if (allowed) {
			window->mImageAnnotator->closeRasterBacking([window]() {
				if (!window) { return; }
				window->mTrayIcon->hide();
				QCoreApplication::exit(0);
			});
		} else {
			window->mQuitPending = false;
			window->refreshImageEnablements();
			window->scheduleInputPump();
		}
	});
}

void MainWindow::processCapture(const CaptureDto &capture)
{
	const auto ownsReservation = !mImageInputReserved;
	if (ownsReservation && !reserveImageInput(false, capture.isValid())) {
		notifyImageInputBusy();
		return;
	}
	processCapture(capture, false);
	if (ownsReservation) {
		releaseImageInput();
	}
}

void MainWindow::processScreenshotCapture(const CaptureDto &capture)
{
	if (mCaptureHandler->isBusy()) {
		// Exactly one admitted capture can finish while a tab operation is busy.
		// Keep that frame, not a queue of decoded captures.
		mDeferredScreenshot = std::make_unique<CaptureDto>(capture);
		scheduleInputPump();
		return;
	}
	auto hideEditor = mHideEditorAfterCaptureEnabled && !mActionProcessor->isActionInProgress();
	processCapture(capture, hideEditor);
	releaseImageInput();
}

void MainWindow::processCapture(const CaptureDto &capture, bool hideEditor)
{
	if (!capture.isValid()) {
		auto title = tr("Unable to show image");
		auto message = tr("No image provided but one was expected.");
		NotifyOperation operation(title, message, NotificationTypes::Critical, mNotificationService, mConfig);
		operation.execute();
		hideEditor ? mVisibilityHandler->enforceHidden() : showEmpty();
		return;
	}

	processImage(capture, hideEditor);
	capturePostProcessing(hideEditor);
}

void MainWindow::processImage(const CaptureDto &capture)
{
	const auto ownsReservation = !mImageInputReserved;
	if (ownsReservation && !reserveImageInput(false, capture.isValid())) {
		notifyImageInputBusy();
		return;
	}
	processImage(capture, false);
	if (ownsReservation) {
		releaseImageInput();
	}
}

void MainWindow::processImage(const CaptureDto &capture, bool hideEditor)
{
	mCaptureHandler->load(capture);
	mHasAcceptedImage = true;
	if (!mActionProcessor->isActionInProgress()) { 	// The action processor handles the showing of the main window
		if (hideEditor) {
			mVisibilityHandler->enforceHidden();
			mWindowResizer->resize();
		} else {
			showDefault();
		}
	}
	captureChanged();
	setEnablements(true);
	emit imageLoaded();
}

DragContent MainWindow::dragContent() const
{
	if (!canUseCurrentImage()) {
		return DragContent();
	}
	return DragContent(mCaptureHandler->image(), mCaptureHandler->path(), mCaptureHandler->isSaved());
}

void MainWindow::resizeToContent()
{
	if(!mToolBar->isCollapsed() || mImageAnnotator->isVisible()) {
		mMainLayout->setSizeConstraint(QLayout::SetFixedSize); // Workaround that allows us to return to toolbar only size
		QMainWindow::adjustSize();
	} else {
		setFixedSize(menuBar()->sizeHint());
	}
	mMainLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
	setMinimumSize(minimumSizeHint()); // Workaround for issue #588
}

bool MainWindow::isWindowMaximized()
{
	return mVisibilityHandler->isMaximized();
}

void MainWindow::capturePostProcessing(bool hideEditor)
{
	if (mConfig->autoCopyToClipboardNewCaptures()) {
		copyCaptureToClipboard();
	}

	if (mConfig->autoSaveNewCaptures()) {
		mCaptureHandler->save(!hideEditor);
	}
}

void MainWindow::showEmpty()
{
	mVisibilityHandler->show();
	captureChanged();
	setEnablements(false);
}

void MainWindow::showHidden()
{
	captureChanged();
	setEnablements(false);
	mVisibilityHandler->hide();
}

void MainWindow::showDefault()
{
	auto enforceVisible = mConfig->showMainWindowAfterTakingScreenshotEnabled();
	enforceVisible ? mVisibilityHandler->enforceVisible() : mVisibilityHandler->restoreState();

	mWindowResizer->resize();
}

void MainWindow::showEditor()
{
	mVisibilityHandler->enforceVisible();
	mWindowResizer->resize();
}

QMenu* MainWindow::createPopupMenu()
{
	// Filtering out the option to hide main toolbar which should no be allowed.
	auto filteredMenu = QMainWindow::createPopupMenu();
	filteredMenu->removeAction(mToolBar->toggleViewAction());
	return filteredMenu;
}

QSize MainWindow::sizeHint() const
{
	auto minHeight = mToolBar->sizeHint().height();
	auto minWidth = mToolBar->sizeHint().width();
	auto annotatorSize = mImageAnnotator->sizeHint();
	auto height = minHeight + annotatorSize.height();
	auto width = minWidth > annotatorSize.width() ? minWidth : annotatorSize.width();
	return { width, height };
}

void MainWindow::moveEvent(QMoveEvent* event)
{
	mConfig->setWindowPosition(pos());
	QWidget::moveEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	if (!mSessionManagerRequestedQuit) {
		event->ignore();
	}

	if (mTrayIcon->isVisible() && mConfig->closeToTray()) {
		mVisibilityHandler->hide();
	} else {
		quit();
	}
}

void MainWindow::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::WindowStateChange) {
		if(mResizeOnNormalize && !isMaximized()) {
			mResizeOnNormalize = false;
			resizeToContent();
		} else if(isMinimized() && mTrayIcon->isVisible() && mConfig->minimizeToTray()) {
			event->ignore();
			mVisibilityHandler->hide();
		}
		mVisibilityHandler->updateState();
	}
	QWidget::changeEvent(event);
}

void MainWindow::captureChanged()
{
	refreshImageEnablements();
	updateApplicationTitle();
	scheduleInputPump();
}

void MainWindow::updateApplicationTitle()
{
	auto path = mCaptureHandler->path();
	auto isUnsaved = !mCaptureHandler->isSaved();
	auto applicationTitle = ApplicationTitleProvider::getApplicationTitle(QApplication::applicationName(), path, tr("Unsaved"), isUnsaved);
	setWindowTitle(applicationTitle);
}

void MainWindow::setEnablements(bool enabled)
{
	mHasCapture = enabled;
	refreshImageEnablements();
}

bool MainWindow::canUseCurrentImage() const
{
	return mHasCapture && mImageAnnotator->isCurrentImageReady() && !mCaptureHandler->isBusy();
}

void MainWindow::refreshImageEnablements()
{
	const auto enabled = canUseCurrentImage();
	mPrintAction->setEnabled(enabled);
	mPrintPreviewAction->setEnabled(enabled);
	mUploadAction->setEnabled(enabled);
	mCopyAsDataUriAction->setEnabled(enabled);
	mScaleAction->setEnabled(enabled);
	mRotateAction->setEnabled(enabled);
	mAddWatermarkAction->setEnabled(enabled);
	mToolBar->setCopyActionEnabled(enabled);
	mToolBar->setCropEnabled(enabled);
	mSaveAsAction->setEnabled(enabled);
	mSaveAllAction->setEnabled(mHasCapture && !mCaptureHandler->isBusy());
	mPinAction->setEnabled(enabled);
	mPasteEmbeddedAction->setEnabled(enabled && mImageAnnotator->canAcceptImage() && mClipboard->isPixmap() && mImageAnnotator->isVisible());
	mPasteAction->setEnabled(canAcquireImage() && mClipboard->isPixmap());
	const auto pathEnabled = enabled && mCaptureHandler->isPathValid();
	mToolBar->setSaveActionEnabled(enabled && !mCaptureHandler->isSaved());
	mCopyPathAction->setEnabled(pathEnabled);
	mRenameAction->setEnabled(pathEnabled);
	mOpenDirectoryAction->setEnabled(pathEnabled);
	mRemoveImageAction->setEnabled(pathEnabled);
	mModifyCanvasAction->setEnabled(enabled);
	mCutAction->setEnabled(enabled);
	mActionProcessor->setPostProcessingEnabled(enabled);
	mOcrAction->setEnabled(mPluginManager->isAvailable(PluginType::Ocr) && enabled);
}

void MainWindow::loadSettings()
{
	mToolBar->selectCaptureMode(mConfig->captureMode());
	mToolBar->setCaptureDelay(mConfig->captureDelay() / 1000);

	if(mConfig->autoHideDocks()) {
		toggleDocks();
	}
}

void MainWindow::triggerCapture(CaptureModes captureMode)
{
	mConfig->setCaptureMode(captureMode);

	capture(captureMode, mConfig->captureCursor(), mConfig->captureDelay());
}

void MainWindow::capture(CaptureModes captureMode, bool captureCursor, int delay)
{
	auto isMainWindowVisible = !isMinimized();
	auto adjustedDelay = mDelayHandler->getDelay(delay, isMainWindowVisible);

	captureScreenshot(captureMode, captureCursor, adjustedDelay);
}

void MainWindow::hideMainWindowIfRequired()
{
	if (mConfig->hideMainWindowDuringScreenshot()) {
		mVisibilityHandler->makeInvisible();
	}
}

void MainWindow::toggleDocks()
{
	auto newIsCollapsedState = !mToolBar->isCollapsed();

	mToolBar->setCollapsed(newIsCollapsedState);
	mImageAnnotator->setSettingsCollapsed(newIsCollapsedState);

	auto collapsedToggleText = newIsCollapsedState ? tr("Show Docks") : tr("Hide Docks");
	mToggleDocksAction->setText(collapsedToggleText);

	mWindowResizer->resize();
}

void MainWindow::initGui()
{
	auto iconLoader = mDependencyInjector->get<IIconLoader>();

	setWindowIcon(iconLoader->load(QLatin1String("ksnip")));

	mToolBar = new MainToolBar(
			mImageGrabber->supportedCaptureModes(),
			mImageAnnotator->undoAction(),
			mImageAnnotator->redoAction(),
			iconLoader,
			mDependencyInjector->get<IScaledSizeProvider>());

	connect(mToolBar, &MainToolBar::captureModeSelected, this, &MainWindow::triggerCapture);
	connect(mToolBar, &MainToolBar::saveActionTriggered, this, &MainWindow::saveClicked);
	connect(mToolBar, &MainToolBar::copyActionTriggered, this, &MainWindow::copyCaptureToClipboard);
	connect(mToolBar, &MainToolBar::captureDelayChanged, this, &MainWindow::captureDelayChanged);
	connect(mToolBar, &MainToolBar::cropActionTriggered, mImageAnnotator, &IImageAnnotator::showCropper);

	mSaveAsAction->setText(tr("Save As..."));
	mSaveAsAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_S);
	mSaveAsAction->setIcon(iconLoader->loadForTheme(QLatin1String("saveAs")));
	connect(mSaveAsAction, &QAction::triggered, this, &MainWindow::saveAsClicked);

	mSaveAllAction->setText(tr("Save All"));
	mSaveAllAction->setIcon(iconLoader->loadForTheme(QLatin1String("save")));
	connect(mSaveAllAction, &QAction::triggered, this, &MainWindow::saveAllClicked);

	mUploadAction->setText(tr("Upload"));
	mUploadAction->setToolTip(tr("Upload triggerCapture to external source"));
	mUploadAction->setShortcut(Qt::SHIFT | Qt::Key_U);
	connect(mUploadAction, &QAction::triggered, this, &MainWindow::upload);

	mCopyAsDataUriAction->setText(tr("Copy as data URI"));
	mCopyAsDataUriAction->setToolTip(tr("Copy triggerCapture to system clipboard"));
	connect(mCopyAsDataUriAction, &QAction::triggered, this, &MainWindow::copyAsDataUri);

	mPrintAction->setText(tr("Print"));
	mPrintAction->setToolTip(tr("Opens printer dialog and provide option to print image"));
	mPrintAction->setShortcut(Qt::CTRL | Qt::Key_P);
	mPrintAction->setIcon(QIcon::fromTheme(QLatin1String("document-print")));
	connect(mPrintAction, &QAction::triggered, this, &MainWindow::printClicked);

	mPrintPreviewAction->setText(tr("Print Preview"));
	mPrintPreviewAction->setToolTip(tr("Opens Print Preview dialog where the image "
									   "orientation can be changed"));
	mPrintPreviewAction->setIcon(QIcon::fromTheme(QLatin1String("document-print-preview")));
	connect(mPrintPreviewAction, &QAction::triggered, this, &MainWindow::printPreviewClicked);

	mScaleAction->setText(tr("Scale"));
	mScaleAction->setToolTip(tr("Scale Image"));
	mScaleAction->setShortcut(Qt::SHIFT | Qt::Key_S);
	connect(mScaleAction, &QAction::triggered, this, &MainWindow::showScaleDialog);

	mRotateAction->setText(tr("Rotate"));
	mRotateAction->setToolTip(tr("Rotate Image"));
	mRotateAction->setShortcut(Qt::SHIFT | Qt::Key_O);
	connect(mRotateAction, &QAction::triggered, this, &MainWindow::showRotateDialog);

	mAddWatermarkAction->setText(tr("Add Watermark"));
	mAddWatermarkAction->setToolTip(tr("Add Watermark to captured image. Multiple watermarks can be added."));
	mAddWatermarkAction->setShortcut(Qt::SHIFT | Qt::Key_W);
	connect(mAddWatermarkAction, &QAction::triggered, this, &MainWindow::addWatermark);

	mQuitAction->setText(tr("Quit"));
	mQuitAction->setShortcut(Qt::CTRL | Qt::Key_Q);
	mQuitAction->setIcon(QIcon::fromTheme(QLatin1String("application-exit")));
	connect(mQuitAction, &QAction::triggered, this, &MainWindow::quit);

	mCloseWindowAction->setText(tr("Close Window"));
	mCloseWindowAction->setShortcut(Qt::SHIFT | Qt::Key_Escape);
	mCloseWindowAction->setIcon(QIcon::fromTheme(QLatin1String("window-close")));
	connect(mCloseWindowAction, &QAction::triggered, this, &MainWindow::close);

	mCopyPathAction->setText(tr("Copy Path"));
	connect(mCopyPathAction, &QAction::triggered, mCaptureHandler, &ICaptureHandler::copyPath);

	mRenameAction->setText(tr("Rename"));
	mRenameAction->setShortcut(Qt::Key_F2);
	connect(mRenameAction, &QAction::triggered, mCaptureHandler, &ICaptureHandler::rename);

	mOpenDirectoryAction->setText(tr("Open Directory"));
	connect(mOpenDirectoryAction, &QAction::triggered, mCaptureHandler, &ICaptureHandler::openDirectory);

	mToggleDocksAction->setText(tr("Hide Docks"));
	mToggleDocksAction->setShortcut(Qt::Key_Tab);
	connect(mToggleDocksAction, &QAction::triggered, this, &MainWindow::toggleDocks);

	mSettingsAction->setText(tr("Settings"));
	mSettingsAction->setIcon(QIcon::fromTheme(QLatin1String("emblem-system")));
	mSettingsAction->setShortcut(Qt::ALT | Qt::Key_F7);
	connect(mSettingsAction, &QAction::triggered, this, &MainWindow::showSettingsDialog);

	mConfigureGlobalShortcutsAction->setText(tr("Configure Global Shortcuts..."));
	connect(mConfigureGlobalShortcutsAction, &QAction::triggered, mGlobalHotKeyHandler, &GlobalHotKeyHandler::configureWaylandShortcuts);

	mAboutAction->setText(tr("&About"));
	mAboutAction->setIcon(iconLoader->load(QLatin1String("ksnip")));
	connect(mAboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

	mOpenImageAction->setText(tr("Open"));
	mOpenImageAction->setIcon(QIcon::fromTheme(QLatin1String("document-open")));
	mOpenImageAction->setShortcut(Qt::CTRL | Qt::Key_O);
	connect(mOpenImageAction, &QAction::triggered, this, &MainWindow::showOpenImageDialog);

	mRecentImagesMenu->setTitle(tr("Open &Recent"));
	mRecentImagesMenu->setIcon(QIcon::fromTheme(QLatin1String("document-open")));

	mPasteAction->setText(tr("Paste"));
	mPasteAction->setIcon(iconLoader->loadForTheme(QLatin1String("paste")));
	mPasteAction->setShortcut(Qt::CTRL | Qt::Key_V);
	mPasteAction->setEnabled(mClipboard->isPixmap());
	connect(mPasteAction, &QAction::triggered, this, &MainWindow::pasteFromClipboard);
	connect(mClipboard.data(), &IClipboard::changed, this, &MainWindow::refreshImageEnablements);

	mPasteEmbeddedAction->setText(tr("Paste Embedded"));
	mPasteEmbeddedAction->setIcon(iconLoader->loadForTheme(QLatin1String("pasteEmbedded")));
	mPasteEmbeddedAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_V);
	mPasteEmbeddedAction->setEnabled(mClipboard->isPixmap() && mImageAnnotator->isVisible());
	connect(mPasteEmbeddedAction, &QAction::triggered, this, &MainWindow::pasteEmbeddedFromClipboard);

	mPinAction->setText(tr("Pin"));
	mPinAction->setToolTip(tr("Pin screenshot to foreground in frameless window"));
	mPinAction->setShortcut(Qt::SHIFT | Qt::Key_P);
	mPinAction->setIcon(iconLoader->loadForTheme(QLatin1String("pin")));
	connect(mPinAction, &QAction::triggered, this, &MainWindow::showPinWindow);

	mRemoveImageAction->setText(tr("Delete"));
	mRemoveImageAction->setIcon(iconLoader->loadForTheme(QLatin1String("delete")));
	connect(mRemoveImageAction, &QAction::triggered, mCaptureHandler, &ICaptureHandler::removeImage);

	mModifyCanvasAction->setText(tr("Modify Canvas"));
	connect(mModifyCanvasAction, &QAction::triggered, mImageAnnotator, &IImageAnnotator::showCanvasModifier);

	mCutAction->setText(tr("Cut"));
	mCutAction->setShortcut(Qt::SHIFT | Qt::Key_T);
	connect(mCutAction, &QAction::triggered, mImageAnnotator, &IImageAnnotator::showCutter);

	mActionsMenu->setTitle(tr("Actions"));
	mActionsMenu->setIcon(iconLoader->loadForTheme(QLatin1String("action")));
	connect(mActionsMenu, &ActionsMenu::triggered, this, &MainWindow::actionTriggered);

	mOcrAction->setText(tr("OCR"));
	connect(mOcrAction, &QAction::triggered, this, &MainWindow::showOcrWindow);

	auto menu = menuBar()->addMenu(tr("&File"));
	menu->addAction(mToolBar->newCaptureAction());
	menu->addSeparator();
	menu->addMenu(mActionsMenu);
	menu->addSeparator();
	menu->addAction(mOpenImageAction);
	menu->addMenu(mRecentImagesMenu);
	menu->addAction(mToolBar->saveAction());
	menu->addAction(mSaveAsAction);
	menu->addAction(mSaveAllAction);
	menu->addAction(mUploadAction);
	menu->addSeparator();
	menu->addAction(mPrintAction);
	menu->addAction(mPrintPreviewAction);
	menu->addSeparator();
	menu->addAction(mCloseWindowAction);
	menu->addAction(mQuitAction);
	menu = menuBar()->addMenu(tr("&Edit"));
	menu->addAction(mToolBar->undoAction());
	menu->addAction(mToolBar->redoAction());
	menu->addSeparator();
	menu->addAction(mToolBar->copyToClipboardAction());
	menu->addAction(mCopyAsDataUriAction);
	menu->addAction(mCopyPathAction);
	menu->addAction(mRenameAction);
	menu->addAction(mPasteAction);
	menu->addAction(mPasteEmbeddedAction);
	menu->addSeparator();
	menu->addAction(mToolBar->cropAction());
	menu->addAction(mScaleAction);
	menu->addAction(mRotateAction);
	menu->addAction(mCutAction);
	menu->addAction(mAddWatermarkAction);
	menu->addSeparator();
	menu->addAction(mRemoveImageAction);
	menu = menuBar()->addMenu(tr("&View"));
	menu->addAction(mOpenDirectoryAction);
	menu->addAction(mToggleDocksAction);
	menu->addAction(mModifyCanvasAction);
	menu = menuBar()->addMenu(tr("&Options"));
	menu->addAction(mPinAction);
	menu->addAction(mOcrAction);
	if(mDependencyInjector->get<IPlatformChecker>()->isWayland()) {
		menu->addAction(mConfigureGlobalShortcutsAction);
	}
	menu->addAction(mSettingsAction);
	menu = menuBar()->addMenu(tr("&Help"));
	menu->addAction(mAboutAction);

	addToolBar(mToolBar);

	if(mConfig->useTrayIcon()) {
		connect(mTrayIcon, &TrayIcon::showEditorTriggered, this, &MainWindow::showEditor);
		mTrayIcon->setCaptureActions(mToolBar->captureActions());
		mTrayIcon->setOpenAction(mOpenImageAction);
		mTrayIcon->setSaveAction(mToolBar->saveAction());
		mTrayIcon->setPasteAction(mPasteAction);
		mTrayIcon->setCopyAction(mToolBar->copyToClipboardAction());
		mTrayIcon->setUploadAction(mUploadAction);
		mTrayIcon->setQuitAction(mQuitAction);
		mTrayIcon->setActionsMenu(mActionsMenu);
		mTrayIcon->setEnabled(true);
	}

	setCentralWidget(mImageAnnotator->widget());
}

void MainWindow::copyCaptureToClipboard()
{
	mCaptureHandler->copy();
}

void MainWindow::upload()
{
	if (!canUseCurrentImage()) {
		return;
	}
	auto image = mCaptureHandler->image();
	UploadOperation operation(image, mUploadHandler, mConfig, mDependencyInjector->get<IMessageBoxService>());
	operation.execute();
}

void MainWindow::copyAsDataUri()
{
	if (!canUseCurrentImage()) {
		return;
	}
	auto image = mCaptureHandler->image();
	CopyAsDataUriOperation operation(image, mClipboard, mNotificationService, mConfig);
	operation.execute();
}

void MainWindow::printClicked()
{
	if (!canUseCurrentImage()) {
		return;
	}
	auto savePath = mSavePathProvider->savePathWithFormat(QLatin1String("pdf"));
	auto image = mCaptureHandler->image();
	mCapturePrinter->print(image, savePath);
}

void MainWindow::printPreviewClicked()
{
	if (!canUseCurrentImage()) {
		return;
	}
	auto savePath = mSavePathProvider->savePathWithFormat(QLatin1String("pdf"));
	auto image = mCaptureHandler->image();
	mCapturePrinter->printPreview(image, savePath);
}

void MainWindow::showOpenImageDialog()
{
	auto title = tr("Open Images");
	auto directory = mSavePathProvider->saveDirectory();
	auto filter = tr("Image Files") + FileDialogFilterHelper::ImageFilesImport();
	auto pathList = mFileDialogService->getOpenFileNames(this, title, directory, filter);

	for (const auto &path : pathList) {
		loadImageFromFile(path);
	}
}

void MainWindow::setupImageAnnotator()
{
	mImageAnnotator->setSaveToolSelection(mConfig->rememberToolSelection());
	mImageAnnotator->setSmoothFactor(mConfig->smoothFactor());
	mImageAnnotator->setSmoothPathEnabled(mConfig->smoothPathEnabled());
	mImageAnnotator->setSwitchToSelectToolAfterDrawingItem(mConfig->switchToSelectToolAfterDrawingItem());
	mImageAnnotator->setSelectItemAfterDrawing(mConfig->selectItemAfterDrawing());
	mImageAnnotator->setNumberToolSeedChangeUpdatesAllItems(mConfig->numberToolSeedChangeUpdatesAllItems());
	mImageAnnotator->setStickers(mConfig->stickerPaths(), mConfig->useDefaultSticker());
	mImageAnnotator->setCanvasColor(mConfig->canvasColor());
	mImageAnnotator->setControlsWidgetVisible(mConfig->isControlsWidgetVisible());
}

void MainWindow::captureDelayChanged(int delay)
{
	mConfig->setCaptureDelay(delay * 1000);
}

void MainWindow::addWatermark()
{
	if (!canUseCurrentImage() || !reserveImageInput(false)) {
		return;
	}
	AddWatermarkOperation operation(mImageAnnotator, mConfig);
	operation.execute();
	releaseImageInput();
}

void MainWindow::showDialog(const std::function<void ()>& showDialogMethod)
{
	mGlobalHotKeyHandler->setEnabled(false);
	showDialogMethod();
	mGlobalHotKeyHandler->setEnabled(true);
}

void MainWindow::showSettingsDialog()
{
	showDialog([&](){
		SettingsDialog settingsDialog(
				mImageGrabber->supportedCaptureModes(),
				mConfig,
				mDependencyInjector->get<IScaledSizeProvider>(),
				mDependencyInjector->get<IDirectoryPathProvider>(),
				mDependencyInjector->get<IFileDialogService>(),
				mDependencyInjector->get<IPlatformChecker>(),
				mDependencyInjector->get<IPluginFinder>(),
				this);
		settingsDialog.exec();
	});
}

void MainWindow::showAboutDialog()
{
	showDialog([&](){
		AboutDialog aboutDialog(this);
		aboutDialog.exec();
	});
}

void MainWindow::showScaleDialog()
{
	if (!canUseCurrentImage()) {
		return;
	}
	showDialog([&](){
		mImageAnnotator->showScaler();
	});
}

void MainWindow::showRotateDialog()
{
	if (!canUseCurrentImage()) {
		return;
	}
	showDialog([&](){
		mImageAnnotator->showRotator();
	});
}

void MainWindow::pasteFromClipboard()
{
	if (!reserveImageInput(false)) {
		notifyImageInputBusy();
		return;
	}
	auto pixmap = mClipboard->pixmap();

	if (mClipboard->url().isNull()) {
		loadImageFromPixmap(pixmap);
	} else if (!pixmap.isNull()) {
		CaptureFromFileDto captureDto(pixmap, mClipboard->url());
		processImage(captureDto);
	}
	releaseImageInput();
}

void MainWindow::pasteEmbeddedFromClipboard()
{
	if (!canUseCurrentImage() || !reserveImageInput(false)) {
		return;
	}
	auto pixmap = mClipboard->pixmap();
	mCaptureHandler->insertImageItem({}, pixmap);
	releaseImageInput();
}

void MainWindow::saveClicked()
{
	mCaptureHandler->save(true);
}

void MainWindow::saveAsClicked()
{
	mCaptureHandler->saveAs();
}

void MainWindow::saveAllClicked()
{
	mCaptureHandler->saveAll();
}

void MainWindow::loadImageFromFile(const QString &path)
{
	openImageFile(path);
}

void MainWindow::openImageFile(const QString &path, bool withCapturePostProcessing)
{
	if (!path.isEmpty()) {
		mPendingImageFiles.enqueue({path, withCapturePostProcessing});
		scheduleInputPump();
	}
}

void MainWindow::openImageData(const QByteArray &data)
{
	if (!reserveImageInput(false)) {
		notifyImageInputBusy();
		return;
	}
	QPixmap pixmap;
	pixmap.loadFromData(data);
	processCapture(CaptureDto(pixmap), false);
	releaseImageInput();
}

void MainWindow::loadTemporaryImageFile(const QString &path)
{
	// A drop source may remove this file when the drop returns: decode now,
	// under admission, or reject it. Never enqueue a borrowed temporary path.
	if (!reserveImageInput(false)) {
		notifyImageInputBusy();
		return;
	}
	loadImageFromPixmap(QPixmap(path));
	releaseImageInput();
}

bool MainWindow::canAcquireImage() const
{
	return !mImageInputReserved && !mCapturePending && !mQuitPending && !mQuitAfterInput &&
		!mCaptureHandler->isBusy() && mImageAnnotator->canAcceptImage();
}

bool MainWindow::reserveImageInput(bool checkReplacement, bool permitInitialImage)
{
	// CLI startup can already own its first screenshot before the editor/cache
	// exists. Keep that single image usable for Save/Copy if cache setup failed;
	// subsequent producers still cannot acquire new pixels without admission.
	const auto initialFallback = permitInitialImage && !mHasAcceptedImage &&
		!mImageInputReserved && !mCapturePending && !mQuitPending && !mQuitAfterInput &&
		!mCaptureHandler->isBusy() && !mImageAnnotator->rasterBackingEnabled() &&
		!mImageAnnotator->canAcceptImage();
	if (!canAcquireImage() && !initialFallback) {
		return false;
	}
	mImageInputReserved = true;
	if (!initialFallback && !mImageAnnotator->reserveImageInput()) {
		mImageInputReserved = false;
		return false;
	}
	if (checkReplacement && !mCaptureHandler->canTakeNew()) {
		releaseImageInput();
		return false;
	}
	refreshImageEnablements();
	return true;
}

void MainWindow::releaseImageInput()
{
	mImageAnnotator->releaseImageInput();
	mImageInputReserved = false;
	refreshImageEnablements();
	if (mQuitAfterInput) {
		QTimer::singleShot(0, this, [this] {
			mQuitAfterInput = false;
			quit();
		});
	} else {
		scheduleInputPump();
	}
}

void MainWindow::scheduleInputPump()
{
	if (mInputPumpScheduled || (mPendingImageFiles.isEmpty() && !mDeferredScreenshot)) {
		return;
	}
	mInputPumpScheduled = true;
	QTimer::singleShot(0, this, [this] {
		mInputPumpScheduled = false;
		processNextImageInput();
	});
}

void MainWindow::processNextImageInput()
{
	if (mDeferredScreenshot) {
		if (!mCaptureHandler->isBusy()) {
			auto capture = std::move(mDeferredScreenshot);
			processScreenshotCapture(*capture);
		}
		return;
	}
	if (mPendingImageFiles.isEmpty() || !reserveImageInput(false)) {
		return;
	}
	const auto input = mPendingImageFiles.dequeue();
	if (input.withCapturePostProcessing) {
		const QPixmap pixmap(input.path);
		const CaptureFromFileDto capture(pixmap, input.path);
		processCapture(capture, false);
	} else {
		LoadImageFromFileOperation operation(input.path, this, mNotificationService,
			mDependencyInjector->get<IRecentImageService>(), mDependencyInjector->get<IFileService>(), mConfig);
		operation.execute();
	}
	releaseImageInput();
}

void MainWindow::notifyImageInputBusy()
{
	NotifyOperation operation(tr("Screenshot input is busy"),
		tr("Wait for the current capture or backing-storage operation to finish, then try again."),
		NotificationTypes::Warning, mNotificationService, mConfig);
	operation.execute();
}

void MainWindow::loadImageFromPixmap(const QPixmap &pixmap)
{
	if (!pixmap.isNull()) {
		CaptureDto captureDto(pixmap);
		processImage(captureDto);
	} else {
		qWarning("Provided pixmap is not valid.");
	}
}

void MainWindow::sessionFinished()
{
	mSessionManagerRequestedQuit = true;
}

void MainWindow::captureCanceled()
{
	mVisibilityHandler->restoreState();
	releaseImageInput();
}

void MainWindow::uploadFinished(const UploadResult &result)
{
	HandleUploadResultOperation handleUploadResponseOperation(result, mNotificationService, mClipboard, mDependencyInjector->get<IDesktopService>(), mConfig);
	handleUploadResponseOperation.execute();
}

void MainWindow::captureEmpty()
{
	setEnablements(false);
	mWindowResizer->resetAndResize();

	if (isWindowMaximized()) {
		mResizeOnNormalize = true;
	}
}

void MainWindow::showPinWindow()
{
	if (!canUseCurrentImage()) {
		return;
	}
	mPinWindowHandler->add(QPixmap::fromImage(mCaptureHandler->image()));
}

void MainWindow::actionTriggered(const Action &action)
{
	if (mActionProcessor->isActionInProgress()) {
		return;
	}
	if(action.isCaptureEnabled() && !canAcquireImage()){
		return;
	}

	mActionProcessor->process(action);
}

void MainWindow::showAfterAction(bool minimized)
{
	if (minimized && mTrayIcon->isVisible()) {
		mVisibilityHandler->hide();
	} else if (minimized) {
		mVisibilityHandler->minimize();
		mVisibilityHandler->restoreState();
	} else {
		mVisibilityHandler->restoreState();
	}

	mWindowResizer->resize();
}

void MainWindow::showOcrWindow()
{
	if (!canUseCurrentImage()) {
		return;
	}
	mOcrWindowHandler->add(QPixmap::fromImage(mCaptureHandler->image()));
}
