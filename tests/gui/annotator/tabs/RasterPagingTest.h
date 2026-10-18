/* SPDX-License-Identifier: LGPL-3.0-or-later */
#ifndef KIMAGEANNOTATOR_RASTERPAGINGTEST_H
#define KIMAGEANNOTATOR_RASTERPAGINGTEST_H

#include <QtTest>
#include <QCryptographicHash>
#include <memory>
#include <kImageAnnotator/KImageAnnotator.h>
#include "src/gui/annotator/tabs/AnnotationTabWidget.h"
#include "src/annotations/items/AnnotationBlur.h"
#include "src/annotations/items/AnnotationDuplicate.h"
#include "src/annotations/items/AnnotationPixelate.h"
#include "src/annotations/items/AnnotationRect.h"
#include "tests/utils/TestRunner.h"
#include "tests/mocks/MockDefaultParameters.h"

using namespace kImageAnnotator;

class RasterPagingTest : public QObject
{
	Q_OBJECT
private slots:
	void init();
	void cleanup();
	void OpenTabs_Should_KeepOneDecodedDocumentAndDeduplicate100LivePages();
	void OpenTabs_Should_KeepOneDecodedDocumentAndDeduplicate100LivePages_data();
	void Admission_Should_HonorOneReservedInput_When_StorageFailsDuringCapture();
	void Paging_Should_PreserveInactiveEffectGeometry();
	void Paging_Should_PreserveFiveTransformUndoGeometryDprSelectionAndZoom();
	void Paging_Should_TrackLateDuplicateAndObfuscationResourcesWithoutRecalculation();
	void Paste_Should_LoadOnlyClipboardResourcesAndSurviveSourceClose();
	void Paste_Should_PreserveDuplicateSnapshotButRecomputeBlurForTarget();
	void Paste_Should_CancelWhenClipboardGenerationOrTargetMembershipChanges();
	void ColdExport_Should_BackNewPaintResourcesBeforeReleasingLease();
	void Requests_Should_AlwaysFinishQueuedAndInvalidateRemovedPages();
	void Batch_Should_AllowExplicitColdExportsWithoutActivatingNeighbors();
	void Switching_Should_CoalesceLatestTargetAndKeepLastCopyOnWriteFailure();
	void Undo_Should_ReleaseTruncatedRedoResources();
	void PublicApi_Should_KeepNewInputReadyAndLegacyModeUnchanged();
	void Shutdown_Should_RemoveSessionAfterDrainingWorkerAndCancelingRequests();

private:
	std::unique_ptr<QTemporaryDir> mDirectory;
	static QPixmap pattern(int seed, qreal dpr = 1.0, QSize size = QSize(129, 97));
	static QByteArray imageDigest(const QImage &image);
	static void initSettings(MockSettingsProvider &settings);
	static RasterPixmapItem *background(AnnotationArea *area);
	static void copyItem(AnnotationTabWidget &tabs, AbstractAnnotationItem *item);
};

#endif
