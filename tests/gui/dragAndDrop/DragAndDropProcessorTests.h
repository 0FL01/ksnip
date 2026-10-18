/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef KSNIP_DRAGANDDROPPROCESSORTESTS_H
#define KSNIP_DRAGANDDROPPROCESSORTESTS_H

#include <QtTest>

#include "src/gui/dragAndDrop/DragAndDropProcessor.h"
#include "tests/utils/TestRunner.h"

class DragAndDropProcessorTests : public QObject
{
	Q_OBJECT
private slots:
	void Drop_Should_RejectTemporaryInputBeforeDecode_When_AdmissionIsDenied();
	void Drop_Should_PassBorrowedTemporaryPathSynchronously_When_AdmissionIsAvailable();
	void Drop_Should_RecheckAdmissionBetweenTemporaryInputs();
	void Drop_Should_PassOrdinaryPathsWithoutDecoding_When_AdmissionIsBusy();
};

#endif // KSNIP_DRAGANDDROPPROCESSORTESTS_H
