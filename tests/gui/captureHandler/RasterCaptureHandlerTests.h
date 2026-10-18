/*
 * Copyright (C) 2026 KSnip contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef KSNIP_RASTERCAPTUREHANDLERTESTS_H
#define KSNIP_RASTERCAPTUREHANDLERTESTS_H

#include <QtTest>
#include <QTabBar>
#include <QTabWidget>
#include <memory>

#include "src/gui/captureHandler/CaptureTabStateHandler.h"
#include "src/gui/captureHandler/MultiCaptureHandler.h"
#include "src/gui/captureHandler/SingleCaptureHandler.h"
#include "tests/utils/TestRunner.h"
#include "tests/mocks/gui/imageAnnotator/ImageAnnotatorMock.h"
#include "tests/mocks/gui/NotificationServiceMock.h"
#include "tests/mocks/gui/clipboard/ClipboardMock.h"
#include "tests/mocks/gui/desktopService/DesktopServiceMock.h"
#include "tests/mocks/gui/messageBoxService/MessageBoxServiceMock.h"
#include "tests/mocks/backend/config/ConfigMock.h"
#include "tests/mocks/backend/saver/ImageSaverMock.h"
#include "tests/mocks/backend/saver/SavePathProviderMock.h"
#include "tests/mocks/backend/recentImages/RecentImageServiceMock.h"
#include "tests/mocks/common/loader/IconLoaderMock.h"

class RasterImageAnnotatorStub : public testing::NiceMock<ImageAnnotatorMock>
{
public:
	bool backing = true;
	bool ready = true;
	bool admission = true;
	int batchesStarted = 0;
	int batchesEnded = 0;
	QList<QPair<int, quint64>> requests;
	QList<quint64> canceledRequests;
	QList<QAction *> actions;

	bool rasterBackingEnabled() const override { return backing; }
	bool isCurrentImageReady() const override { return ready; }
	bool canAcceptImage() const override { return admission; }
	void requestImageAt(int index, quint64 requestId) override { requests.append(qMakePair(index, requestId)); }
	void cancelImageRequest(quint64 requestId) override { canceledRequests.append(requestId); }
	void beginTabBatch() override { ++batchesStarted; }
	void endTabBatch() override { ++batchesEnded; }
	void finishRequest(quint64 requestId, const QImage &image, bool canceled = false, const QString &error = QString())
	{
		QMetaObject::invokeMethod(this, [this, requestId, image, canceled, error] {
			emit imageRequestFinished(requestId, image, canceled, error);
		}, Qt::QueuedConnection);
	}
};

class RasterCaptureHandlerFixture
{
public:
	QWidget parent;
	RasterImageAnnotatorStub annotator;
	QImage image{2, 2, QImage::Format_ARGB32};
	QSharedPointer<CaptureTabStateHandler> tabs{new CaptureTabStateHandler};
	QSharedPointer<testing::NiceMock<NotificationServiceMock>> notifications{new testing::NiceMock<NotificationServiceMock>};
	QSharedPointer<testing::NiceMock<ConfigMock>> config{new testing::NiceMock<ConfigMock>};
	QSharedPointer<testing::NiceMock<ClipboardMock>> clipboard{new testing::NiceMock<ClipboardMock>};
	QSharedPointer<testing::NiceMock<DesktopServiceMock>> desktop{new testing::NiceMock<DesktopServiceMock>};
	QSharedPointer<testing::NiceMock<MessageBoxServiceMock>> messages{new testing::NiceMock<MessageBoxServiceMock>};
	QSharedPointer<testing::NiceMock<RecentImageServiceMock>> recent{new testing::NiceMock<RecentImageServiceMock>};
	QSharedPointer<testing::NiceMock<ImageSaverMock>> saver{new testing::NiceMock<ImageSaverMock>};
	QSharedPointer<testing::NiceMock<SavePathProviderMock>> paths{new testing::NiceMock<SavePathProviderMock>};
	QSharedPointer<testing::NiceMock<IconLoaderMock>> icons{new testing::NiceMock<IconLoaderMock>};
	std::unique_ptr<MultiCaptureHandler> handler;

	RasterCaptureHandlerFixture();
	quint64 add(bool saved = false);
	void select(int index);
	void trigger(const QString &text, int index);
	void deliver(quint64 requestId, bool canceled = false, const QString &error = QString());
};

class RasterCaptureHandlerTests : public QObject
{
	Q_OBJECT
private slots:
	void TabMoved_Should_ShiftIntermediateDocuments_When_MoveIsNotAdjacent();
	void TabMoved_Should_KeepCurrentDocument_When_RealTabWidgetSignalsAreDelivered();
	void SetSaveStateIfRevision_Should_RejectOldRevision_And_NeverReuseClosedIdentity();
	void CurrentTabChanged_Should_NotChangeRevisionOrSaveState();
	void Save_Should_CompleteSynchronouslyBeforeCopyAndOpenDirectory_When_CurrentImageReady();
	void Load_Should_InsertCursorBeforeSynchronousAutoSave_When_NewCaptureReady();
	void Save_Should_FollowDocumentAcrossTabMove_When_ColdExportCompletes();
	void Save_Should_KeepNewRevisionDirty_When_DocumentEditedDuringExport();
	void Copy_Should_RequestOnlyColdDocument_And_FollowIdentityAcrossMove();
	void SaveAll_Should_ExportSequentially_And_ContinueAfterFailureOrCancellation();
	void CloseBatch_Should_NotExportPixels_When_SavedOrDiscardedOrCanceled();
	void SingleCanClose_Should_NotExportPixels_When_SavedOrDiscardedOrCanceled();
	void CloseBatch_Should_KeepNewRevisionOpen_When_EditedDuringSave();
	void CanClose_Should_KeepNewRevisionOpen_When_LegacySaveDispatchesEdit();
	void RequestClose_Should_StopOnFirstVeto_And_FinishOnce_When_ExportCanceled();
	void RequestClose_Should_CloseSavedDocumentsWithoutPixels_And_StopOnMetadataVeto();
	void CancelPendingOperations_Should_CancelRequestAndReleaseBatch_And_IgnoreLateResult();
	void PendingCommand_Should_BlockConflictingOperations_And_NotifyListener();
	void Destruction_Should_CancelOutstandingRequest_And_FinishContinuation();
};

#endif //KSNIP_RASTERCAPTUREHANDLERTESTS_H
