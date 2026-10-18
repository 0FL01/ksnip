/* SPDX-License-Identifier: LGPL-3.0-or-later */
#ifndef KIMAGEANNOTATOR_RASTERSTORETEST_H
#define KIMAGEANNOTATOR_RASTERSTORETEST_H

#include <QtTest>
#include <QFileInfo>
#include "src/backend/RasterStore.h"
#include "tests/utils/TestRunner.h"

using namespace kImageAnnotator;

class RasterStoreTest : public QObject
{
	Q_OBJECT
private slots:
	void Prepare_Should_DeduplicateMeaningfulRowsAndRestoreMetadata();
	void Prepare_Should_CompareCandidatesAndRejectCorruptReads();
	void Prepare_Should_KeepLastDecodedCopy_When_WriteFails();
	void Lifetime_Should_RemoveBlobAndSessionAfterLastConsumer();
	void Prepare_Should_RejectInvalidDescriptorWithoutPublishingPixels();
	void Prepare_Should_PreservePaletteAndIgnoreUnusedMonoBits_data();
	void Prepare_Should_PreservePaletteAndIgnoreUnusedMonoBits();
	void Prepare_Should_DeduplicateIgnoredPixelBits_data();
	void Prepare_Should_DeduplicateIgnoredPixelBits();
	void Prepare_Should_NotDiscardRealAlpha_data();
	void Prepare_Should_NotDiscardRealAlpha();
	void Prepare_Should_NotMergeEqualPixelsWithDifferentMetadata();
	void Lifetime_Should_KeepBlobUntilConsumerOutlivesStore();
	void Initialize_Should_RejectMemoryBackedFilesystems_data();
	void Initialize_Should_RejectMemoryBackedFilesystems();
	void Initialize_Should_RejectSymlinkToMemoryBackedFilesystem();
	void Initialize_Should_UseOneDiskBackedDefaultFallback();
	void Initialize_Should_FailWhenBothDefaultLocationsAreMemoryBacked();

private:
	static QImage pattern(uchar padding);
	static QString directoryTemplate();
	static QImage pixelPattern(QImage::Format format);
	static QByteArray rawDigest(const QImage &image);
	static void poisonIgnoredBits(QImage &image, uchar value);
	static bool isMemoryBacked(const QStorageInfo &storage);
	static QString memoryBackedDirectory();
};

#endif
