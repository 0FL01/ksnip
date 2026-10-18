/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef KSNIP_KIMAGEANNOTATORADAPTERTESTS_H
#define KSNIP_KIMAGEANNOTATORADAPTERTESTS_H

#include <QtTest>
#include <QTemporaryDir>

#include "src/gui/imageAnnotator/KImageAnnotatorAdapter.h"
#include "tests/utils/TestRunner.h"

class KImageAnnotatorAdapterTests : public QObject
{
	Q_OBJECT
private slots:
	void Backing_Should_FollowTheActualLibraryCapability();
	void Capture_Should_BeSynchronouslyExportableIncludingCursor_When_BackingIsPending();
	void RequestImageAt_Should_ForwardACompleteImageWithoutSwitchingTabs();
	void Backing_Should_RejectAcquisition_When_InitializationFails();
	void Shutdown_Should_CompleteAfterBackingFilesAreRemoved();
};

#endif // KSNIP_KIMAGEANNOTATORADAPTERTESTS_H
