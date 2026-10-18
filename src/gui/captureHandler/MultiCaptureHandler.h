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

#ifndef KSNIP_MULTICAPTUREHANDLER_H
#define KSNIP_MULTICAPTUREHANDLER_H

#include <QPointer>
#include <algorithm>

#include "src/gui/captureHandler/ICaptureHandler.h"
#include "src/gui/captureHandler/ICaptureTabStateHandler.h"
#include "src/gui/captureHandler/TabContextMenuAction.h"
#include "src/gui/operations/CanDiscardOperation.h"
#include "src/gui/operations/SaveOperation.h"
#include "src/gui/operations/RenameOperation.h"
#include "src/gui/operations/DeleteImageOperation.h"
#include "src/gui/INotificationService.h"
#include "src/gui/clipboard/IClipboard.h"
#include "src/gui/desktopService/IDesktopService.h"
#include "src/gui/imageAnnotator/IImageAnnotator.h"
#include "src/common/provider/NewCaptureNameProvider.h"
#include "src/common/provider/PathFromCaptureProvider.h"
#include "src/common/loader/IIconLoader.h"
#include "src/dependencyInjector/DependencyInjector.h"

class MultiCaptureHandler : public ICaptureHandler
{
Q_OBJECT
public:
	explicit MultiCaptureHandler(
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
			QWidget *parent);
	~MultiCaptureHandler() override;
	bool canClose() override;
	void requestClose(const std::function<void(bool)> &finished) override;
	bool isBusy() const override;
	void cancelPendingOperations() override;
	bool canTakeNew() override;
	bool isSaved() const override;
	QString path() const override;
	bool isPathValid() const override;
	void saveAs() override;
	void save(bool showSuccessToast) override;
	void saveAll() override;
	void rename() override;
	void copy() override;
	void copyPath() override;
	void openDirectory() override;
	void removeImage() override;
	void load(const CaptureDto &capture) override;
	QImage image() const override;
	void insertImageItem(const QPointF &pos, const QPixmap &pixmap) override;
	void addListener(ICaptureChangeListener *captureChangeListener) override;

private:
	enum class Command { None, Save, Copy, SaveAll, CloseTabs, Quit };
	struct PendingCommand
	{
		Command command = Command::None;
		QList<quint64> documents;
		int next = 0;
		quint64 serial = 0;
		quint64 document = 0;
		quint64 revision = 0;
		quint64 request = 0;
		bool isInstant = true;
		bool showSuccessToast = true;
		bool tabBatch = false;
		bool successful = true;
		std::function<void(bool)> finished;
	};

	QPointer<IImageAnnotator> mImageAnnotator;
	QSharedPointer<ICaptureTabStateHandler> mTabStateHandler;
	QSharedPointer<INotificationService> mNotificationService;
	QWidget *mParent;
	ICaptureChangeListener *mCaptureChangeListener;
	NewCaptureNameProvider mNewCaptureNameProvider;
	PathFromCaptureProvider mPathFromCaptureProvider;
	QSharedPointer<IConfig> mConfig;
	QSharedPointer<IClipboard> mClipboard;
	QSharedPointer<IDesktopService> mDesktopService;
	QSharedPointer<IMessageBoxService> mMessageBoxService;
	QSharedPointer<IFileService> mFileService;
	QSharedPointer<IRecentImageService> mRecentImageService;
	QSharedPointer<IImageSaver> mImageSaver;
	QSharedPointer<ISavePathProvider> mSavePathProvider;
	QSharedPointer<IFileDialogService> mFileDialogService;
	TabContextMenuAction *mSaveContextMenuAction;
	TabContextMenuAction *mSaveAsContextMenuAction;
	TabContextMenuAction *mSaveAllContextMenuAction;
	TabContextMenuAction *mRenameContextMenuAction;
	TabContextMenuAction *mOpenDirectoryContextMenuAction;
	TabContextMenuAction *mCopyPathToClipboardContextMenuAction;
	TabContextMenuAction *mCopyToClipboardContextMenuAction;
	TabContextMenuAction *mDeleteImageContextMenuAction;
	QAction *mContextMenuSeparatorAction;
	PendingCommand mPending;
	quint64 mNextCommandId = 1;
	quint64 mNextRequestId = 1;

	bool discardChanges(int index);
	void removeTab(int currentTabIndex);
	bool saveAt(int index, bool isInstant, bool showSuccessToast);
	bool saveImageAt(int index, const QImage &image, bool isInstant, bool showSuccessToast);
	bool startCommand(Command command, const QList<quint64> &documents, bool isInstant = true,
			bool showSuccessToast = true, const std::function<void(bool)> &finished = {});
	void processPending();
	bool completeImageCommand(const QImage &image);
	bool completeEntry(bool successful);
	void finishCommand(bool successful);
	bool pendingDocumentIsCurrentRevision() const;
	void reportImageError(const QString &error);
	void addTabContextMenuActions(const QSharedPointer<IIconLoader> &iconLoader);

private slots:
	void tabCloseRequested(int index);
	void tabCloseBatchRequested(const QList<int> &indices);
	void imageRequestFinished(quint64 requestId, const QImage &image, bool canceled, const QString &error);
	void captureChanged();
	void captureEmpty();
	void annotatorConfigChanged();
	void saveAsTab(int index);
	void renameTab(int index);
	void saveTab(int index);
	void saveAllTabs();
	void openDirectoryTab(int index);
	void updateContextMenuActions(int index);
	void copyToClipboardTab(int index);
	void copyPathToClipboardTab(int index);
	void deleteImageTab(int index);
};

#endif //KSNIP_MULTICAPTUREHANDLER_H
