/*
 * Copyright (C) 2026 KSnip contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "RasterCaptureHandlerTests.h"

RasterCaptureHandlerFixture::RasterCaptureHandlerFixture()
{
	image.fill(Qt::red);
	ON_CALL(annotator, image()).WillByDefault(testing::Return(image));
	ON_CALL(annotator, addTabContextMenuActions(testing::_)).WillByDefault([this](const QList<QAction *> &actions) {
		annotator.actions = actions;
	});
	ON_CALL(annotator, removeTab(testing::_)).WillByDefault([this](int index) {
		auto current = tabs->currentTabIndex();
		auto remaining = tabs->count() - 1;
		auto next = current > index ? current - 1 : qMin(current, remaining - 1);
		emit annotator.currentTabChanged(next);
		if (annotator.batchesStarted != annotator.batchesEnded) {
			annotator.ready = false;
		}
	});
	ON_CALL(*config, promptSaveBeforeExit()).WillByDefault(testing::Return(true));
	ON_CALL(*paths, savePath()).WillByDefault(testing::Return(QStringLiteral("/captures/recovered.png")));
	ON_CALL(*saver, save(testing::_, testing::_)).WillByDefault(testing::Return(true));
	EXPECT_CALL(annotator, imageAt(testing::_)).Times(0);
	handler = std::make_unique<MultiCaptureHandler>(&annotator, notifications, tabs, config, clipboard, desktop,
			nullptr, messages, recent, saver, paths, icons, nullptr, &parent);
}

quint64 RasterCaptureHandlerFixture::add(bool saved)
{
	auto index = tabs->count();
	auto name = QStringLiteral("capture-%1").arg(index);
	tabs->add(index, name, QStringLiteral("/captures/%1.png").arg(name), saved);
	return tabs->documentId(index);
}

void RasterCaptureHandlerFixture::select(int index)
{
	emit annotator.currentTabChanged(index);
}

void RasterCaptureHandlerFixture::trigger(const QString &text, int index)
{
	for (auto action : annotator.actions) {
		if (action->text() == text) {
			action->setData(index);
			action->trigger();
			return;
		}
	}
	QFAIL("Missing tab context menu action");
}

void RasterCaptureHandlerFixture::deliver(quint64 requestId, bool canceled, const QString &error)
{
	annotator.finishRequest(requestId, error.isEmpty() && !canceled ? image : QImage(), canceled, error);
	QCoreApplication::sendPostedEvents(&annotator, QEvent::MetaCall);
}

void RasterCaptureHandlerTests::TabMoved_Should_ShiftIntermediateDocuments_When_MoveIsNotAdjacent()
{
	CaptureTabStateHandler states;
	QList<quint64> identities;
	for (int index = 0; index < 5; ++index) {
		states.add(index, QString::number(index), QString(), index % 2 == 0);
		identities.append(states.documentId(index));
	}
	states.currentTabChanged(2);
	states.tabMoved(0, 4);
	QCOMPARE(states.documentId(0), identities.at(1));
	QCOMPARE(states.documentId(1), identities.at(2));
	QCOMPARE(states.documentId(2), identities.at(3));
	QCOMPARE(states.documentId(3), identities.at(4));
	QCOMPARE(states.documentId(4), identities.at(0));
	QCOMPARE(states.currentTabIndex(), 1);
	states.tabMoved(4, 1);
	QCOMPARE(states.documentId(0), identities.at(1));
	QCOMPARE(states.documentId(1), identities.at(0));
	QCOMPARE(states.documentId(2), identities.at(2));
	QCOMPARE(states.documentId(3), identities.at(3));
	QCOMPARE(states.documentId(4), identities.at(4));
	QCOMPARE(states.currentTabIndex(), 2);
}

void RasterCaptureHandlerTests::TabMoved_Should_KeepCurrentDocument_When_RealTabWidgetSignalsAreDelivered()
{
	for (int selected = 0; selected < 4; ++selected) {
		for (int from = 0; from < 4; ++from) {
			for (int to = 0; to < 4; ++to) {
				QTabWidget widget;
				CaptureTabStateHandler states;
				connect(&widget, &QTabWidget::currentChanged, &states, &CaptureTabStateHandler::currentTabChanged);
				connect(widget.tabBar(), &QTabBar::tabMoved, &states, &CaptureTabStateHandler::tabMoved);
				for (int index = 0; index < 4; ++index) {
					auto name = QString::number(index);
					widget.addTab(new QWidget, name);
					states.add(index, name, QString(), false);
				}
				widget.setCurrentIndex(selected);
				auto document = states.documentId(selected);
				widget.tabBar()->moveTab(from, to);
				QCOMPARE(states.currentTabIndex(), widget.currentIndex());
				QCOMPARE(states.documentId(states.currentTabIndex()), document);
				for (int index = 0; index < 4; ++index) {
					QCOMPARE(states.filename(index), widget.tabText(index));
				}
			}
		}
	}
}

void RasterCaptureHandlerTests::SetSaveStateIfRevision_Should_RejectOldRevision_And_NeverReuseClosedIdentity()
{
	CaptureTabStateHandler states;
	states.add(0, QStringLiteral("capture"), QString(), false);
	states.currentTabChanged(0);
	auto document = states.documentId(0);
	auto revision = states.revision(0);
	states.currentTabContentChanged();
	QVERIFY(!states.setSaveStateIfRevision(document, revision, SaveResultDto(true, QStringLiteral("old.png"))));
	QVERIFY(!states.isSaved(0));
	QVERIFY(states.path(0).isEmpty());
	QVERIFY(states.setSaveStateIfRevision(document, revision + 1, SaveResultDto(true, QStringLiteral("new.png"))));
	QVERIFY(states.isSaved(0));
	states.tabRemoved(0);
	states.add(0, QStringLiteral("capture"), QString(), false);
	QVERIFY(states.documentId(0) != document);
	QVERIFY(!states.setSaveStateIfRevision(document, revision + 1, SaveResultDto(true, QStringLiteral("old.png"))));
	QVERIFY(!states.isSaved(0));
}

void RasterCaptureHandlerTests::CurrentTabChanged_Should_NotChangeRevisionOrSaveState()
{
	CaptureTabStateHandler states;
	states.add(0, QStringLiteral("one"), QStringLiteral("one.png"), true);
	states.add(1, QStringLiteral("two"), QString(), false);
	states.currentTabChanged(0);
	states.currentTabChanged(1);
	states.currentTabChanged(0);
	QCOMPARE(states.revision(0), quint64(0));
	QCOMPARE(states.revision(1), quint64(0));
	QVERIFY(states.isSaved(0));
	QVERIFY(!states.isSaved(1));
}

void RasterCaptureHandlerTests::Save_Should_CompleteSynchronouslyBeforeCopyAndOpenDirectory_When_CurrentImageReady()
{
	RasterCaptureHandlerFixture fixture;
	fixture.add();
	fixture.select(0);
	fixture.annotator.admission = false;
	testing::InSequence sequence;
	EXPECT_CALL(*fixture.saver, save(testing::_, QStringLiteral("/captures/capture-0.png"))).WillOnce(testing::Return(true));
	EXPECT_CALL(*fixture.clipboard, setImage(fixture.image));
	EXPECT_CALL(*fixture.desktop, openFile(QStringLiteral("/captures")));
	fixture.handler->save(false);
	QVERIFY(!fixture.handler->isBusy());
	QVERIFY(fixture.tabs->isSaved(0));
	fixture.handler->copy();
	fixture.handler->openDirectory();
	QVERIFY(fixture.annotator.requests.isEmpty());
}

void RasterCaptureHandlerTests::Load_Should_InsertCursorBeforeSynchronousAutoSave_When_NewCaptureReady()
{
	RasterCaptureHandlerFixture fixture;
	CaptureDto capture(QPixmap::fromImage(fixture.image));
	capture.cursor.image = QPixmap::fromImage(fixture.image);
	capture.cursor.position = QPoint(1, 1);
	testing::InSequence sequence;
	EXPECT_CALL(fixture.annotator, addTab(testing::_, testing::_, testing::_)).WillOnce([&fixture](const QPixmap &, const QString &, const QString &) {
		fixture.select(0);
		return 0;
	});
	EXPECT_CALL(fixture.annotator, insertImageItem(QPointF(1, 1), testing::_)).WillOnce([&fixture](const QPointF &, const QPixmap &) {
		emit fixture.annotator.imageChanged();
	});
	EXPECT_CALL(*fixture.saver, save(testing::_, testing::_)).WillOnce(testing::Return(true));
	fixture.handler->load(capture);
	QCOMPARE(fixture.tabs->revision(0), quint64(1));
	fixture.handler->save(false);
	QVERIFY(fixture.tabs->isSaved(0));
	QVERIFY(!fixture.handler->isBusy());
	QVERIFY(fixture.annotator.requests.isEmpty());
}

void RasterCaptureHandlerTests::Save_Should_FollowDocumentAcrossTabMove_When_ColdExportCompletes()
{
	RasterCaptureHandlerFixture fixture;
	auto document = fixture.add();
	fixture.add();
	fixture.add();
	fixture.select(2);
	fixture.trigger(QStringLiteral("Save"), 0);
	QVERIFY(fixture.handler->isBusy());
	QCOMPARE(fixture.annotator.requests.count(), 1);
	emit fixture.annotator.tabMoved(0, 2);
	fixture.select(1);
	EXPECT_CALL(*fixture.saver, save(testing::_, QStringLiteral("/captures/capture-0.png"))).WillOnce(testing::Return(true));
	fixture.deliver(fixture.annotator.requests.last().second);
	QVERIFY(!fixture.handler->isBusy());
	QCOMPARE(fixture.tabs->indexForDocument(document), 2);
	QVERIFY(fixture.tabs->isSaved(2));
	QVERIFY(!fixture.tabs->isSaved(0));
}

void RasterCaptureHandlerTests::Save_Should_KeepNewRevisionDirty_When_DocumentEditedDuringExport()
{
	RasterCaptureHandlerFixture fixture;
	fixture.add();
	fixture.add();
	fixture.select(1);
	fixture.trigger(QStringLiteral("Save"), 0);
	fixture.select(0);
	emit fixture.annotator.imageChanged();
	emit fixture.annotator.tabMoved(0, 1);
	fixture.select(0);
	EXPECT_CALL(*fixture.saver, save(testing::_, testing::_)).Times(0);
	fixture.deliver(fixture.annotator.requests.last().second);
	QVERIFY(!fixture.handler->isBusy());
	QVERIFY(!fixture.tabs->isSaved(1));
	QCOMPARE(fixture.tabs->revision(1), quint64(1));
}

void RasterCaptureHandlerTests::SaveAll_Should_ExportSequentially_And_ContinueAfterFailureOrCancellation()
{
	RasterCaptureHandlerFixture fixture;
	fixture.add();
	fixture.add();
	fixture.add();
	fixture.add();
	fixture.select(3);
	EXPECT_CALL(*fixture.saver, save(testing::_, QStringLiteral("/captures/capture-0.png"))).WillOnce(testing::Return(false));
	EXPECT_CALL(*fixture.saver, save(testing::_, QStringLiteral("/captures/capture-3.png"))).WillOnce(testing::Return(true));
	fixture.handler->saveAll();
	QCOMPARE(fixture.annotator.requests.count(), 1);
	fixture.deliver(fixture.annotator.requests.last().second);
	QCOMPARE(fixture.annotator.requests.count(), 2);
	QVERIFY(fixture.handler->isBusy());
	fixture.deliver(fixture.annotator.requests.last().second, true);
	QCOMPARE(fixture.annotator.requests.count(), 3);
	fixture.deliver(fixture.annotator.requests.last().second, false, QStringLiteral("read failed"));
	QVERIFY(!fixture.handler->isBusy());
	QVERIFY(!fixture.tabs->isSaved(0));
	QVERIFY(!fixture.tabs->isSaved(1));
	QVERIFY(!fixture.tabs->isSaved(2));
	QVERIFY(fixture.tabs->isSaved(3));
}

void RasterCaptureHandlerTests::Copy_Should_RequestOnlyColdDocument_And_FollowIdentityAcrossMove()
{
	RasterCaptureHandlerFixture fixture;
	fixture.add();
	fixture.add();
	fixture.select(1);
	EXPECT_CALL(fixture.annotator, image()).Times(0);
	EXPECT_CALL(*fixture.clipboard, setImage(fixture.image));
	fixture.trigger(QStringLiteral("Copy"), 0);
	QCOMPARE(fixture.annotator.requests.count(), 1);
	QVERIFY(fixture.handler->isBusy());
	emit fixture.annotator.tabMoved(0, 1);
	fixture.select(0);
	fixture.deliver(fixture.annotator.requests.last().second);
	QVERIFY(!fixture.handler->isBusy());
}

void RasterCaptureHandlerTests::CloseBatch_Should_NotExportPixels_When_SavedOrDiscardedOrCanceled()
{
	RasterCaptureHandlerFixture fixture;
	fixture.add();
	auto canceled = fixture.add();
	fixture.add(true);
	fixture.select(2);
	EXPECT_CALL(fixture.annotator, image()).Times(0);
	EXPECT_CALL(*fixture.messages, yesNoCancel(testing::_, testing::_))
			.WillOnce(testing::Return(MessageBoxResponse::Cancel))
			.WillOnce(testing::Return(MessageBoxResponse::No));
	emit fixture.annotator.tabCloseBatchRequested({0, 2, 1});
	QVERIFY(fixture.annotator.requests.isEmpty());
	QCOMPARE(fixture.tabs->count(), 1);
	QCOMPARE(fixture.tabs->documentId(0), canceled);
	QCOMPARE(fixture.annotator.batchesStarted, 1);
	QCOMPARE(fixture.annotator.batchesEnded, 1);
	QVERIFY(!fixture.handler->isBusy());
}

void RasterCaptureHandlerTests::SingleCanClose_Should_NotExportPixels_When_SavedOrDiscardedOrCanceled()
{
	RasterCaptureHandlerFixture fixture;
	fixture.handler.reset();
	SingleCaptureHandler handler(&fixture.annotator, fixture.notifications, fixture.clipboard, fixture.desktop,
			nullptr, fixture.messages, fixture.recent, fixture.saver, fixture.paths, nullptr, fixture.config, &fixture.parent);
	EXPECT_CALL(fixture.annotator, image()).Times(0);
	QVERIFY(handler.canClose());
	handler.load(CaptureDto(QPixmap::fromImage(fixture.image)));
	EXPECT_CALL(*fixture.messages, yesNoCancel(testing::_, testing::_))
			.WillOnce(testing::Return(MessageBoxResponse::No))
			.WillOnce(testing::Return(MessageBoxResponse::Cancel));
	QVERIFY(handler.canClose());
	QVERIFY(!handler.canClose());
	QVERIFY(fixture.annotator.requests.isEmpty());
}

void RasterCaptureHandlerTests::CloseBatch_Should_KeepNewRevisionOpen_When_EditedDuringSave()
{
	RasterCaptureHandlerFixture fixture;
	auto document = fixture.add();
	fixture.select(0);
	EXPECT_CALL(*fixture.messages, yesNoCancel(testing::_, testing::_)).WillOnce(testing::Return(MessageBoxResponse::Yes));
	EXPECT_CALL(*fixture.saver, save(testing::_, testing::_)).WillOnce([&fixture](const QImage &, const QString &) {
		emit fixture.annotator.imageChanged();
		return true;
	});
	emit fixture.annotator.tabCloseRequested(0);
	QCOMPARE(fixture.tabs->count(), 1);
	QCOMPARE(fixture.tabs->documentId(0), document);
	QVERIFY(!fixture.tabs->isSaved(0));
	QVERIFY(!fixture.handler->isBusy());
	QCOMPARE(fixture.annotator.batchesEnded, 1);
}

void RasterCaptureHandlerTests::CanClose_Should_KeepNewRevisionOpen_When_LegacySaveDispatchesEdit()
{
	RasterCaptureHandlerFixture fixture;
	fixture.annotator.backing = false;
	fixture.add();
	fixture.select(0);
	EXPECT_CALL(fixture.annotator, imageAt(0)).WillOnce(testing::Return(fixture.image));
	EXPECT_CALL(*fixture.messages, yesNoCancel(testing::_, testing::_)).WillOnce(testing::Return(MessageBoxResponse::Yes));
	EXPECT_CALL(*fixture.saver, save(testing::_, testing::_)).WillOnce([&fixture](const QImage &, const QString &) {
		emit fixture.annotator.imageChanged();
		return true;
	});
	QVERIFY(!fixture.handler->canClose());
	QCOMPARE(fixture.tabs->count(), 1);
	QVERIFY(!fixture.tabs->isSaved(0));
}

void RasterCaptureHandlerTests::RequestClose_Should_StopOnFirstVeto_And_FinishOnce_When_ExportCanceled()
{
	int finished = 0;
	bool allowed = true;
	RasterCaptureHandlerFixture fixture;
	fixture.add();
	fixture.add(true);
	fixture.select(1);
	EXPECT_CALL(*fixture.messages, yesNoCancel(testing::_, testing::_)).WillOnce(testing::Return(MessageBoxResponse::Yes));
	fixture.handler->requestClose([&finished, &allowed](bool successful) { ++finished; allowed = successful; });
	QCOMPARE(fixture.tabs->count(), 1);
	QVERIFY(fixture.handler->isBusy());
	QCOMPARE(fixture.annotator.requests.count(), 1);
	auto request = fixture.annotator.requests.last().second;
	fixture.deliver(request, true);
	fixture.deliver(request);
	QCOMPARE(finished, 1);
	QVERIFY(!allowed);
	QCOMPARE(fixture.tabs->count(), 1);
	QVERIFY(!fixture.handler->isBusy());
	QCOMPARE(fixture.annotator.batchesEnded, 1);
}

void RasterCaptureHandlerTests::RequestClose_Should_CloseSavedDocumentsWithoutPixels_And_StopOnMetadataVeto()
{
	int finished = 0;
	bool allowed = false;
	RasterCaptureHandlerFixture fixture;
	fixture.add(true);
	fixture.add(true);
	fixture.add(true);
	fixture.select(1);
	EXPECT_CALL(fixture.annotator, image()).Times(0);
	fixture.handler->requestClose([&finished, &allowed](bool successful) { ++finished; allowed = successful; });
	QCOMPARE(finished, 1);
	QVERIFY(allowed);
	QCOMPARE(fixture.tabs->count(), 0);
	QVERIFY(fixture.annotator.requests.isEmpty());
	fixture.add();
	fixture.add(true);
	fixture.add();
	fixture.select(1);
	EXPECT_CALL(*fixture.messages, yesNoCancel(testing::_, testing::_)).WillOnce(testing::Return(MessageBoxResponse::Cancel));
	fixture.handler->requestClose([&finished, &allowed](bool successful) { ++finished; allowed = successful; });
	QCOMPARE(finished, 2);
	QVERIFY(!allowed);
	QCOMPARE(fixture.tabs->count(), 2);
	QVERIFY(fixture.annotator.requests.isEmpty());
	QVERIFY(!fixture.handler->isBusy());
	QCOMPARE(fixture.annotator.batchesStarted, 2);
	QCOMPARE(fixture.annotator.batchesEnded, 2);
}

void RasterCaptureHandlerTests::CancelPendingOperations_Should_CancelRequestAndReleaseBatch_And_IgnoreLateResult()
{
	RasterCaptureHandlerFixture fixture;
	fixture.add();
	fixture.add();
	fixture.select(1);
	EXPECT_CALL(*fixture.messages, yesNoCancel(testing::_, testing::_)).WillOnce(testing::Return(MessageBoxResponse::Yes));
	emit fixture.annotator.tabCloseRequested(0);
	auto request = fixture.annotator.requests.last().second;
	fixture.handler->cancelPendingOperations();
	QVERIFY(!fixture.handler->isBusy());
	QCOMPARE(fixture.annotator.canceledRequests, QList<quint64>{request});
	QCOMPARE(fixture.annotator.batchesEnded, 1);
	EXPECT_CALL(*fixture.saver, save(testing::_, testing::_)).Times(0);
	fixture.deliver(request);
	QCOMPARE(fixture.tabs->count(), 2);
	QVERIFY(!fixture.tabs->isSaved(0));
}

void RasterCaptureHandlerTests::PendingCommand_Should_BlockConflictingOperations_And_NotifyListener()
{
	class Listener : public ICaptureChangeListener
	{
	public:
		int changes = 0;
		void captureChanged() override { ++changes; }
		void captureEmpty() override {}
	};
	bool duplicateQuit = true;
	RasterCaptureHandlerFixture fixture;
	Listener listener;
	fixture.handler->addListener(&listener);
	fixture.add();
	fixture.add();
	fixture.select(1);
	auto before = listener.changes;
	fixture.trigger(QStringLiteral("Save"), 0);
	QCOMPARE(listener.changes, before + 1);
	fixture.handler->saveAll();
	fixture.handler->save(false);
	fixture.handler->saveAs();
	fixture.handler->copy();
	fixture.handler->rename();
	fixture.handler->removeImage();
	EXPECT_CALL(fixture.annotator, addTab(testing::_, testing::_, testing::_)).Times(0);
	fixture.handler->load(CaptureDto(QPixmap::fromImage(fixture.image)));
	emit fixture.annotator.tabCloseRequested(1);
	fixture.handler->requestClose([&duplicateQuit](bool allowed) { duplicateQuit = allowed; });
	QVERIFY(!duplicateQuit);
	QCOMPARE(fixture.annotator.requests.count(), 1);
	QCOMPARE(fixture.tabs->count(), 2);
	fixture.deliver(fixture.annotator.requests.last().second);
	QVERIFY(listener.changes > before + 1);
	QVERIFY(!fixture.handler->isBusy());
	fixture.handler->addListener(nullptr);
}

void RasterCaptureHandlerTests::Destruction_Should_CancelOutstandingRequest_And_FinishContinuation()
{
	int finished = 0;
	RasterCaptureHandlerFixture fixture;
	fixture.add();
	fixture.select(0);
	fixture.annotator.ready = false;
	EXPECT_CALL(*fixture.messages, yesNoCancel(testing::_, testing::_)).WillOnce(testing::Return(MessageBoxResponse::Yes));
	fixture.handler->requestClose([&finished](bool allowed) { QVERIFY(!allowed); ++finished; });
	auto request = fixture.annotator.requests.last().second;
	fixture.handler.reset();
	QCOMPARE(finished, 1);
	QCOMPARE(fixture.annotator.canceledRequests, QList<quint64>{request});
	QCOMPARE(fixture.annotator.batchesEnded, 1);
	fixture.deliver(request);
	QCOMPARE(finished, 1);
}

TEST_MAIN(RasterCaptureHandlerTests)
