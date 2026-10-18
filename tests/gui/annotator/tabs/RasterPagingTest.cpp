/* SPDX-License-Identifier: LGPL-3.0-or-later */
#include "RasterPagingTest.h"

namespace {

class CountingBlur : public AnnotationBlur
{
public:
	CountingBlur(const QPointF &position, const ObfuscatePropertiesPtr &properties) : AnnotationBlur(position, properties) {}
	mutable int computations = 0;

protected:
	QImage obfuscateBackground(const QImage &image) const override
	{
		++computations;
		return AnnotationBlur::obfuscateBackground(image);
	}
};

}

void RasterPagingTest::init()
{
	mDirectory = std::make_unique<QTemporaryDir>(QDir::currentPath() + QStringLiteral("/build-raster-paging-XXXXXX"));
	QVERIFY(mDirectory->isValid());
}

void RasterPagingTest::cleanup()
{
	// Session cleanup belongs to the worker. Keep its parent alive until it ends.
	QTRY_VERIFY(QDir(mDirectory->path()).entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot).isEmpty());
	mDirectory.reset();
}

QPixmap RasterPagingTest::pattern(int seed, qreal dpr, QSize size)
{
	QImage image(size, QImage::Format_ARGB32_Premultiplied);
	for (int y = 0; y < image.height(); ++y) {
		for (int x = 0; x < image.width(); ++x) {
			image.setPixel(x, y, qRgb((x * 29 + seed * 17) % 256, (y * 31 + seed * 43) % 256, (x * 7 + y * 13 + seed) % 256));
		}
	}
	image.setDevicePixelRatio(dpr);
	image.setColorSpace(QColorSpace(QColorSpace::SRgb));
	return QPixmap::fromImage(image);
}

QByteArray RasterPagingTest::imageDigest(const QImage &image)
{
	QCryptographicHash hash(QCryptographicHash::Sha256);
	hash.addData(RasterDescriptor(image).canonical());
	for (int y = 0; y < image.height(); ++y) {
		hash.addData(reinterpret_cast<const char *>(image.constScanLine(y)), image.width() * image.depth() / 8);
	}
	return hash.result();
}

void RasterPagingTest::initSettings(MockSettingsProvider &settings)
{
	qRegisterMetaType<QList<int>>();
	settings.setToolType(Tools::Select);
	settings.setToolColor(Qt::red);
	settings.setTextColor(Qt::blue);
	settings.setToolWidth(2);
	settings.setFillType(FillModes::BorderAndNoFill);
	settings.setBlurRadius(5);
	settings.setShadowEnabled(false);
}

RasterPixmapItem *RasterPagingTest::background(AnnotationArea *area)
{
	for (auto item : area->items()) {
		if (auto raster = dynamic_cast<RasterPixmapItem *>(item)) { return raster; }
	}
	return nullptr;
}

void RasterPagingTest::copyItem(AnnotationTabWidget &tabs, AbstractAnnotationItem *item)
{
	AnnotationItemModifier selection(tabs.currentZoomValueProvider());
	selection.selectItem(item);
	tabs.findChild<AnnotationItemClipboard *>()->copyItems(QPointF(), &selection);
}

void RasterPagingTest::OpenTabs_Should_KeepOneDecodedDocumentAndDeduplicate100LivePages_data()
{
	QTest::addColumn<int>("uniqueResources");
	QTest::newRow("100-identical") << 1;
	QTest::newRow("100-overlapping") << 7;
	QTest::newRow("100-distinct") << 100;
}

void RasterPagingTest::OpenTabs_Should_KeepOneDecodedDocumentAndDeduplicate100LivePages()
{
	QFETCH(int, uniqueResources);
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	QSignalSpy errors(&tabs, &AnnotationTabWidget::rasterBackingError);
	QVector<QByteArray> expected;
	for (int i = 0; i < 100; ++i) {
		QTRY_VERIFY(tabs.canAcceptImage());
		QCOMPARE(tabs.addTab(pattern(i % uniqueResources), QString::number(i), QString()), i);
		QVERIFY(tabs.isCurrentImageReady());
		QVERIFY(!tabs.readyImageAt(i).isNull());
		expected.append(imageDigest(tabs.readyImageAt(i)));
	}
	QTRY_VERIFY(tabs.canAcceptImage());
	QCOMPARE(tabs.count(), 100);
	for (int i = 0; i < 100; ++i) {
		auto area = tabs.annotationAreaAt(i);
		QVERIFY(area);
		QCOMPARE(area->rasterDocument()->isResident(), i == 99);
		for (const auto &resource : area->rasterDocument()->handles()) {
			QVERIFY(resource->blob);
			QCOMPARE(resource->image.isNull(), i != 99);
		}
		QVERIFY(background(area));
		QCOMPARE(background(area)->pixmap().isNull(), i != 99);
		QVERIFY(background(area)->graphicsEffect());
	}
	auto store = tabs.findChild<RasterStore *>();
	QVERIFY(store);
	QCOMPARE(QDir(store->sessionDirectory()).entryList(QDir::Files).size(), uniqueResources);
	for (int index : {0, 37, 12, 99, 1}) {
		tabs.setCurrentIndex(index);
		QTRY_VERIFY(tabs.isCurrentImageReady());
		QCOMPARE(imageDigest(tabs.readyImageAt(index)), expected.at(index));
	}
	QCOMPARE(errors.count(), 0);
	QSignalSpy batches(&tabs, &AnnotationTabWidget::tabCloseBatchRequested);
	QSignalSpy legacyClose(&tabs, &AnnotationTabWidget::tabCloseRequested);
	auto selected = tabs.currentIndex();
	tabs.findChild<AnnotationTabCloser *>()->closeAllTabsTriggered();
	QCOMPARE(batches.count(), 1);
	QCOMPARE(legacyClose.count(), 0);
	QCOMPARE(tabs.currentIndex(), selected);
	auto indices = batches.first().first().value<QList<int>>();
	QCOMPARE(indices.size(), 100);
	QCOMPARE(indices.first(), 99);
	QCOMPARE(indices.last(), 0);
	tabs.beginTabBatch();
	for (int index : indices) { tabs.removeTab(index); }
	QVERIFY(!tabs.isCurrentImageReady());
	tabs.endTabBatch();
	QTRY_VERIFY(tabs.canAcceptImage());
	QCOMPARE(tabs.count(), 0);
}

void RasterPagingTest::Admission_Should_HonorOneReservedInput_When_StorageFailsDuringCapture()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	tabs.addTab(pattern(1), QString(), QString());
	QTRY_VERIFY(tabs.canAcceptImage());
	QVERIFY(tabs.reserveImageInput());
	QVERIFY(!tabs.canAcceptImage());
	QVERIFY(!tabs.reserveImageInput());
	tabs.releaseImageInput(); // A canceled acquisition returns its credit.
	QTRY_VERIFY(tabs.canAcceptImage());
	QVERIFY(tabs.reserveImageInput());
	auto source = tabs.currentAnnotationArea();
	auto store = tabs.findChild<RasterStore *>();
	auto original = store->sessionDirectory();
	auto displaced = original + QStringLiteral("-away");
	QVERIFY(QDir().rename(original, displaced));
	source->flip(FlipDirection::Vertical);
	auto expectedSource = imageDigest(source->image());
	QSignalSpy errors(&tabs, &AnnotationTabWidget::rasterBackingError);
	QTRY_VERIFY(!errors.isEmpty());
	QVERIFY(!tabs.canAcceptImage());
	QCOMPARE(tabs.addTab(pattern(2), QString(), QString()), 1);
	QVERIFY(tabs.isCurrentImageReady());
	QVERIFY(!tabs.readyImageAt(1).isNull());
	QVERIFY(source->rasterDocument()->needsBacking());
	bool retained = false;
	for (const auto &handle : source->rasterDocument()->handles()) {
		if (!handle->blob) { retained = true; QVERIFY(!handle->image.isNull()); }
	}
	QVERIFY(retained);
	QCOMPARE(tabs.addTab(pattern(3), QString(), QString()), -1);
	QCOMPARE(tabs.count(), 2); // No unbounded RAM fallback after the one credit.
	errors.clear(); // Do not confuse the rejected third input with the next I/O failure.
	tabs.releaseImageInput();
	QTRY_VERIFY(!errors.isEmpty());
	auto incoming = tabs.currentAnnotationArea();
	auto resources = incoming->rasterDocument()->handles().size();
	incoming->flip(FlipDirection::Horizontal); // Known failure blocks new raster producers too.
	QCOMPARE(incoming->rasterDocument()->handles().size(), resources);
	QVERIFY(!tabs.readyImageAt(1).isNull()); // Recovery Save/Copy remains usable.
	QVERIFY(QDir().rename(displaced, original));
	tabs.setCurrentIndex(0); // Explicit retry, without producing more rasters.
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QTRY_VERIFY(tabs.canAcceptImage());
	tabs.setCurrentIndex(1);
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QTRY_VERIFY(tabs.canAcceptImage());
	for (const auto &handle : source->rasterDocument()->handles()) { QVERIFY(handle->image.isNull()); }
	tabs.setCurrentIndex(0);
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QCOMPARE(imageDigest(tabs.readyImageAt(0)), expectedSource);
}

void RasterPagingTest::Paging_Should_PreserveInactiveEffectGeometry()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	tabs.addTab(pattern(1), QString(), QString());
	auto area = tabs.currentAnnotationArea();
	area->imageEffectChanged(ImageEffects::Border);
	auto expectedCanvas = area->canvasRect();
	auto expectedEffectBounds = background(area)->graphicsEffect()->boundingRect();
	QTRY_VERIFY(tabs.canAcceptImage());
	tabs.addTab(pattern(2), QString(), QString());
	QTRY_VERIFY(tabs.canAcceptImage());
	QCOMPARE(area->canvasRect(), expectedCanvas);
	QCOMPARE(background(area)->graphicsEffect()->boundingRect(), expectedEffectBounds);
	tabs.setCurrentIndex(0);
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QCOMPARE(area->canvasRect(), expectedCanvas);
}

void RasterPagingTest::Paging_Should_PreserveFiveTransformUndoGeometryDprSelectionAndZoom()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	tabs.addTab(pattern(1, 1.5), QString(), QString());
	auto area = tabs.currentAnnotationArea();
	QVERIFY(background(area));
	QCOMPARE(background(area)->raster().handle()->descriptor.devicePixelRatio, qreal(1.5));
	auto rect = new AnnotationRect(QPointF(11, 14), PropertiesPtr(new AnnotationProperties(Qt::red, 2)));
	rect->addPoint(QPointF(31, 37), false);
	rect->init();
	area->addAnnotationItem(rect);
	auto selection = area->findChild<AnnotationItemModifier *>();
	// The modifier is a graphics item, not necessarily an QObject child.
	if (!selection) {
		for (auto item : area->items()) {
			if (auto modifier = dynamic_cast<AnnotationItemModifier *>(item)) { selection = modifier; break; }
		}
	}
	QVERIFY(selection);
	selection->selectItem(rect);
	tabs.currentZoomValueProvider()->setZoomValue(1.375);
	const auto expectedZoom = tabs.currentZoomValueProvider()->zoomValue();
	QVector<QByteArray> states;
	states.append(imageDigest(area->image()));
	area->crop(QRectF(2, 3, 70, 50));
	states.append(imageDigest(area->image()));
	area->scale(QSize(111, 83));
	states.append(imageDigest(area->image()));
	area->rotate(90);
	states.append(imageDigest(area->image()));
	area->flip(FlipDirection::Horizontal);
	states.append(imageDigest(area->image()));
	auto canvas = area->backgroundImageRect();
	area->cut(QRectF(canvas.left() + 5, canvas.top(), 7, canvas.height()));
	states.append(imageDigest(area->image()));
	auto geometry = rect->boundingRect();
	auto sceneRect = area->sceneRect();
	QTRY_VERIFY(tabs.canAcceptImage());
	tabs.addTab(pattern(9), QString(), QString());
	QTRY_VERIFY(tabs.canAcceptImage());
	for (const auto &resource : area->rasterDocument()->handles()) { QVERIFY(resource->image.isNull()); }
	QSignalSpy edits(&tabs, &AnnotationTabWidget::imageChanged);
	tabs.setCurrentIndex(0);
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QCOMPARE(edits.count(), 0);
	QCOMPARE(rect->boundingRect(), geometry);
	QCOMPARE(area->sceneRect(), sceneRect);
	QCOMPARE(selection->selectedItems(), QList<AbstractAnnotationItem *>({rect}));
	QCOMPARE(tabs.currentZoomValueProvider()->zoomValue(), expectedZoom);
	QCOMPARE(imageDigest(area->image()), states.last());
	for (int i = states.size() - 2; i >= 0; --i) {
		area->undoAction()->trigger();
		QVERIFY(!edits.isEmpty());
		edits.clear();
		QCOMPARE(imageDigest(area->image()), states.at(i));
	}
	QCOMPARE(background(area)->raster().handle()->descriptor.devicePixelRatio, qreal(1.5));
	for (int i = 1; i < states.size(); ++i) {
		area->redoAction()->trigger();
		QVERIFY(!edits.isEmpty());
		edits.clear();
		QCOMPARE(imageDigest(area->image()), states.at(i));
	}
}

void RasterPagingTest::Paging_Should_TrackLateDuplicateAndObfuscationResourcesWithoutRecalculation()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	tabs.addTab(pattern(3), QString(), QString());
	auto area = tabs.currentAnnotationArea();
	auto duplicate = new AnnotationDuplicate(QPointF(10, 10), PropertiesPtr(new AnnotationProperties(Qt::red, 1)));
	duplicate->addPoint(QPointF(40, 40), false);
	duplicate->init();
	area->addAnnotationItem(duplicate);
	auto initialResources = area->rasterDocument()->handles().size();
	duplicate->finish(); // Resource is created AFTER binding the item to its document.
	QCOMPARE(area->rasterDocument()->handles().size(), initialResources + 1);
	auto properties = ObfuscatePropertiesPtr(new AnnotationObfuscateProperties);
	properties->setColor(Qt::red);
	properties->setWidth(1);
	properties->setFillType(FillModes::NoBorderAndFill);
	properties->setOpacity(1.0);
	properties->setFactor(5);
	auto blur = new CountingBlur(QPointF(55, 10), properties);
	blur->addPoint(QPointF(90, 40), false);
	blur->init();
	area->addAnnotationItem(blur);
	auto pixelate = new AnnotationPixelate(QPointF(80, 20), properties);
	pixelate->addPoint(QPointF(109, 43), false);
	pixelate->init();
	area->addAnnotationItem(pixelate);
	auto expected = imageDigest(area->image()); // First export binds both pending masks.
	QVERIFY(blur->computations > 0);
	auto computations = blur->computations;
	QTRY_VERIFY(tabs.canAcceptImage());
	tabs.addTab(pattern(8), QString(), QString());
	QTRY_VERIFY(tabs.canAcceptImage());
	for (const auto &handle : area->rasterDocument()->handles()) { QVERIFY(handle->image.isNull()); }
	tabs.setCurrentIndex(0);
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QCOMPARE(imageDigest(area->image()), expected);
	QCOMPARE(blur->computations, computations); // Missing decoded bytes is not a dirty obfuscation flag.
	QCOMPARE(area->rasterDocument()->handles().size(), initialResources + 3);
}

void RasterPagingTest::Paste_Should_LoadOnlyClipboardResourcesAndSurviveSourceClose()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	tabs.addTab(pattern(1), QStringLiteral("source"), QString());
	auto source = tabs.currentAnnotationArea();
	source->insertImageItem(QPointF(3, 4), pattern(22, 1.0, QSize(23, 17)));
	AbstractAnnotationItem *imageItem = nullptr;
	for (auto item : source->items()) {
		if (auto annotation = dynamic_cast<AnnotationImage *>(item)) { imageItem = annotation; break; }
	}
	QVERIFY(imageItem);
	auto properties = imageItem->properties();
	properties->setOpacity(1.0);
	imageItem->setProperties(properties);
	copyItem(tabs, imageItem);
	QTRY_VERIFY(tabs.canAcceptImage());
	tabs.addTab(pattern(7), QStringLiteral("target"), QString());
	QTRY_VERIFY(tabs.canAcceptImage());
	auto target = tabs.currentAnnotationArea();
	QVERIFY(background(source)->raster().image().isNull());
	QVERIFY(tabs.findChild<AnnotationItemClipboard *>()->rasterHandles().first()->image.isNull());
	target->rasterPasteRequested(QPointF(30, 30));
	QVERIFY(!tabs.isCurrentImageReady());
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QVERIFY(background(source)->raster().image().isNull()); // Do not preload source history.
	QCOMPARE(target->rasterDocument()->handles().size(), 2);
	auto expected = imageDigest(target->image());
	tabs.removeTab(0);
	QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	QTRY_VERIFY(tabs.canAcceptImage());
	QVERIFY(tabs.findChild<AnnotationItemClipboard *>()->isNotEmpty());
	target->rasterPasteRequested(QPointF(30, 30));
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QCOMPARE(imageDigest(target->image()), expected); // Same snapshot, despite destroyed source.
	target->undoAction()->trigger();
	QCOMPARE(imageDigest(target->image()), expected);
}

void RasterPagingTest::Requests_Should_AlwaysFinishQueuedAndInvalidateRemovedPages()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	tabs.addTab(pattern(1), QString(), QString());
	auto expectedCold = imageDigest(tabs.readyImageAt(0));
	QTRY_VERIFY(tabs.canAcceptImage());
	tabs.addTab(pattern(2), QString(), QString());
	QTRY_VERIFY(tabs.canAcceptImage());
	QSignalSpy results(&tabs, &AnnotationTabWidget::imageRequestFinished);
	tabs.requestImageAt(1, 1);
	QCOMPARE(results.count(), 0);
	QTRY_COMPARE(results.count(), 1);
	QVERIFY(!results.at(0).at(1).value<QImage>().isNull());
	QVERIFY(!results.at(0).at(2).toBool());
	QVERIFY(results.at(0).at(3).toString().isEmpty());
	results.clear(); // Release test-owned raster results before checking residency.
	tabs.requestImageAt(0, 5);
	QCOMPARE(results.count(), 0);
	QTRY_COMPARE(results.count(), 1);
	QCOMPARE(imageDigest(results.at(0).at(1).value<QImage>()), expectedCold);
	QCOMPARE(tabs.currentIndex(), 1);
	QVERIFY(tabs.isCurrentImageReady());
	results.clear();
	for (const auto &handle : tabs.annotationAreaAt(0)->rasterDocument()->handles()) { QVERIFY(handle->image.isNull()); }
	tabs.requestImageAt(0, 2);
	tabs.cancelImageRequest(2);
	QCOMPARE(results.count(), 0);
	QTRY_COMPARE(results.count(), 1);
	QVERIFY(results.at(0).at(2).toBool());
	results.clear();
	tabs.requestImageAt(0, 3);
	QPointer<QWidget> removedPage = tabs.widget(0);
	tabs.removeTab(0);
	QVERIFY(removedPage); // Membership invalidation must precede QObject destruction.
	QTRY_COMPARE(results.count(), 1);
	QVERIFY(results.at(0).at(2).toBool());
	results.clear();
	tabs.requestImageAt(99, 4);
	QCOMPARE(results.count(), 0);
	QTRY_COMPARE(results.count(), 1);
	QVERIFY(!results.at(0).at(3).toString().isEmpty());
	QVERIFY(results.at(0).at(1).value<QImage>().isNull());
	results.clear();
	QVERIFY(tabs.readyImageAt(99).isNull());
}

void RasterPagingTest::Batch_Should_AllowExplicitColdExportsWithoutActivatingNeighbors()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	for (int i = 0; i < 3; ++i) {
		QTRY_VERIFY(tabs.canAcceptImage());
		tabs.addTab(pattern(i), QString(), QString());
	}
	QTRY_VERIFY(tabs.canAcceptImage());
	tabs.beginTabBatch();
	QVERIFY(tabs.isCurrentImageReady());
	tabs.removeTab(2);
	QCOMPARE(tabs.currentIndex(), 1);
	QVERIFY(!tabs.isCurrentImageReady());
	QSignalSpy results(&tabs, &AnnotationTabWidget::imageRequestFinished);
	tabs.requestImageAt(0, 80); // A save-before-close while the batch guard is held.
	QCOMPARE(results.count(), 0);
	QTRY_COMPARE(results.count(), 1);
	QVERIFY(!results.first().at(1).value<QImage>().isNull());
	QVERIFY(results.first().at(3).toString().isEmpty());
	results.clear();
	QCOMPARE(tabs.currentIndex(), 1);
	for (int i = 0; i < 2; ++i) {
		QVERIFY(!tabs.annotationAreaAt(i)->rasterDocument()->isResident());
		for (const auto &handle : tabs.annotationAreaAt(i)->rasterDocument()->handles()) { QVERIFY(handle->image.isNull()); }
	}
	tabs.endTabBatch();
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QCOMPARE(tabs.currentIndex(), 1);
}

void RasterPagingTest::Paste_Should_PreserveDuplicateSnapshotButRecomputeBlurForTarget()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	tabs.addTab(pattern(1), QString(), QString());
	auto source = tabs.currentAnnotationArea();
	auto duplicateProperties = PropertiesPtr(new AnnotationProperties(Qt::red, 1));
	duplicateProperties->setOpacity(1.0);
	auto duplicate = new AnnotationDuplicate(QPointF(5, 5), duplicateProperties);
	duplicate->addPoint(QPointF(35, 35), false);
	duplicate->init();
	source->addAnnotationItem(duplicate);
	duplicate->finish();
	auto properties = ObfuscatePropertiesPtr(new AnnotationObfuscateProperties);
	properties->setWidth(1);
	properties->setFillType(FillModes::NoBorderAndFill);
	properties->setOpacity(1.0);
	properties->setFactor(5);
	auto blur = new CountingBlur(QPointF(60, 10), properties);
	blur->addPoint(QPointF(95, 40), false);
	blur->init();
	source->addAnnotationItem(blur);
	imageDigest(source->image());
	QTRY_VERIFY(tabs.canAcceptImage());
	QByteArray sourceBlurDigest;
	for (const auto &handle : source->rasterDocument()->handles()) {
		if (handle->descriptor.size == QSize(35, 30)) { sourceBlurDigest = handle->blob->digest; }
	}
	QVERIFY(!sourceBlurDigest.isEmpty());
	{
		AnnotationItemModifier selection(tabs.currentZoomValueProvider());
		selection.selectItem(duplicate);
		selection.selectItem(blur);
		tabs.findChild<AnnotationItemClipboard *>()->copyItems(QPointF(), &selection);
	}
	auto clipboardResources = tabs.findChild<AnnotationItemClipboard *>()->rasterHandles();
	QCOMPARE(clipboardResources.size(), 1); // Blur clones intentionally have no source snapshot.
	auto duplicateHandle = clipboardResources.first();
	clipboardResources.clear();
	QTRY_VERIFY(tabs.canAcceptImage());
	tabs.addTab(pattern(17), QString(), QString());
	QTRY_VERIFY(tabs.canAcceptImage());
	QVERIFY(duplicateHandle->image.isNull());
	auto target = tabs.currentAnnotationArea();
	target->rasterPasteRequested(QPointF(0, 0));
	QTRY_VERIFY(tabs.isCurrentImageReady());
	imageDigest(target->image());
	QTRY_VERIFY(tabs.canAcceptImage());
	QVERIFY(target->rasterDocument()->handles().contains(duplicateHandle));
	QByteArray targetBlurDigest;
	for (const auto &handle : target->rasterDocument()->handles()) {
		if (handle->descriptor.size == QSize(35, 30)) { targetBlurDigest = handle->blob->digest; }
	}
	QVERIFY(!targetBlurDigest.isEmpty());
	QVERIFY(targetBlurDigest != sourceBlurDigest);
}

void RasterPagingTest::Paste_Should_CancelWhenClipboardGenerationOrTargetMembershipChanges()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	tabs.addTab(pattern(1), QString(), QString());
	auto source = tabs.currentAnnotationArea();
	source->insertImageItem(QPointF(3, 4), pattern(22, 1.0, QSize(23, 17)));
	AbstractAnnotationItem *sourceItem = nullptr;
	for (auto item : source->items()) {
		if (auto image = dynamic_cast<AnnotationImage *>(item)) { sourceItem = image; }
	}
	QVERIFY(sourceItem);
	copyItem(tabs, sourceItem);
	QTRY_VERIFY(tabs.canAcceptImage());
	tabs.addTab(pattern(7), QString(), QString());
	QTRY_VERIFY(tabs.canAcceptImage());
	auto target = tabs.currentAnnotationArea();
	auto before = imageDigest(target->image());
	target->rasterPasteRequested(QPointF(20, 20));
	// Dispatch the scheduler, not the store's completion receiver. The genuine
	// worker operation is now in flight even on very fast storage.
	QCoreApplication::sendPostedEvents(&tabs, QEvent::MetaCall);
	QVERIFY(tabs.findChild<RasterStore *>()->isBusy());
	tabs.findChild<AnnotationItemClipboard *>()->clear();
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QCOMPARE(target->rasterDocument()->handles().size(), 1);
	QCOMPARE(imageDigest(target->image()), before);
	for (const auto &handle : source->rasterDocument()->handles()) { QVERIFY(handle->image.isNull()); }
	tabs.setCurrentIndex(0);
	QTRY_VERIFY(tabs.isCurrentImageReady());
	copyItem(tabs, sourceItem);
	QTRY_VERIFY(tabs.canAcceptImage());
	tabs.setCurrentIndex(1);
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QTRY_VERIFY(tabs.canAcceptImage());
	QSignalSpy edits(&tabs, &AnnotationTabWidget::imageChanged);
	target->rasterPasteRequested(QPointF(20, 20));
	QCoreApplication::sendPostedEvents(&tabs, QEvent::MetaCall);
	QVERIFY(tabs.findChild<RasterStore *>()->isBusy());
	QPointer<QWidget> page = tabs.widget(1);
	tabs.removeTab(1);
	QVERIFY(page);
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QTRY_VERIFY(tabs.canAcceptImage());
	QCOMPARE(tabs.currentAnnotationArea(), source);
	QCOMPARE(source->rasterDocument()->handles().size(), 2);
	QCOMPARE(edits.count(), 0);
}

void RasterPagingTest::ColdExport_Should_BackNewPaintResourcesBeforeReleasingLease()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	tabs.addTab(pattern(1), QString(), QString());
	auto source = tabs.currentAnnotationArea();
	auto properties = ObfuscatePropertiesPtr(new AnnotationObfuscateProperties);
	properties->setWidth(1);
	properties->setFillType(FillModes::NoBorderAndFill);
	properties->setOpacity(1.0);
	properties->setFactor(5);
	auto blur = new CountingBlur(QPointF(10, 10), properties);
	blur->addPoint(QPointF(45, 40), false);
	blur->init();
	source->addAnnotationItem(blur);
	imageDigest(source->image());
	QTRY_VERIFY(tabs.canAcceptImage());
	auto computations = blur->computations;
	blur->setPosition(QPointF(60, 50)); // Dirty flag + old raster, not yet painted.
	tabs.addTab(pattern(7), QString(), QString());
	QTRY_VERIFY(tabs.canAcceptImage());
	QSignalSpy results(&tabs, &AnnotationTabWidget::imageRequestFinished);
	tabs.requestImageAt(0, 70);
	QTRY_COMPARE(results.count(), 1);
	QVERIFY(results.first().at(3).toString().isEmpty());
	QVERIFY(!results.first().at(1).value<QImage>().isNull());
	results.clear();
	QVERIFY(blur->computations > computations);
	QVERIFY(!source->rasterDocument()->isResident());
	QVERIFY(!source->rasterDocument()->needsBacking());
	for (const auto &handle : source->rasterDocument()->handles()) { QVERIFY(handle->image.isNull()); }
	QVERIFY(tabs.isCurrentImageReady());
	QTRY_VERIFY(tabs.canAcceptImage());
}

void RasterPagingTest::Switching_Should_CoalesceLatestTargetAndKeepLastCopyOnWriteFailure()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	for (int i = 0; i < 4; ++i) {
		QTRY_VERIFY(tabs.canAcceptImage());
		tabs.addTab(pattern(i), QString(), QString());
	}
	QTRY_VERIFY(tabs.canAcceptImage());
	tabs.setCurrentIndex(0);
	QCoreApplication::processEvents(); // Allow one load to start, then supersede it.
	tabs.setCurrentIndex(1);
	tabs.setCurrentIndex(2);
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QCOMPARE(tabs.currentIndex(), 2);
	for (int i = 0; i < 4; ++i) {
		QCOMPARE(tabs.annotationAreaAt(i)->rasterDocument()->isResident(), i == 2);
	}
	QTRY_VERIFY(tabs.canAcceptImage());
	auto store = tabs.findChild<RasterStore *>();
	auto original = store->sessionDirectory();
	auto displaced = original + QStringLiteral("-away");
	QVERIFY(QDir().rename(original, displaced));
	auto area = tabs.currentAnnotationArea();
	area->flip(FlipDirection::Vertical);
	auto expected = imageDigest(area->image());
	QSignalSpy errors(&tabs, &AnnotationTabWidget::rasterBackingError);
	tabs.setCurrentIndex(0);
	QTRY_VERIFY(!errors.isEmpty());
	QCOMPARE(tabs.currentIndex(), 2);
	QVERIFY(tabs.isCurrentImageReady());
	QVERIFY(!tabs.canAcceptImage());
	QCOMPARE(imageDigest(tabs.readyImageAt(2)), expected);
	QVERIFY(QDir().rename(displaced, original));
	tabs.setCurrentIndex(1); // Explicit retry after the storage path is repaired.
	QTRY_VERIFY(tabs.isCurrentImageReady());
	QTRY_VERIFY(tabs.canAcceptImage());
	QCOMPARE(tabs.currentIndex(), 1);
	for (int i = 0; i < 4; ++i) {
		QCOMPARE(tabs.annotationAreaAt(i)->rasterDocument()->isResident(), i == 1);
	}
}

void RasterPagingTest::Undo_Should_ReleaseTruncatedRedoResources()
{
	auto &directory = *mDirectory;
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(directory.path()));
	tabs.addTab(pattern(1), QString(), QString());
	auto area = tabs.currentAnnotationArea();
	area->insertImageItem(QPointF(3, 4), pattern(22, 1.0, QSize(23, 17)));
	QPointer<AbstractAnnotationItem> item;
	QWeakPointer<RasterData> resource;
	for (auto graphic : area->items()) {
		if (auto annotation = dynamic_cast<AnnotationImage *>(graphic)) { item = annotation; }
	}
	for (const auto &handle : area->rasterDocument()->handles()) {
		if (handle->descriptor.size == QSize(23, 17)) { resource = handle; }
	}
	QVERIFY(item);
	QVERIFY(!resource.isNull());
	QTRY_VERIFY(tabs.canAcceptImage());
	area->undoAction()->trigger();
	QVERIFY(item && !item->scene());
	area->crop(QRectF(1, 1, 50, 40)); // Truncates AddCommand redo and its detached item.
	QVERIFY(!item);
	QTRY_VERIFY(resource.isNull());
	QCOMPARE(area->rasterDocument()->handles().size(), 2);
}

void RasterPagingTest::PublicApi_Should_KeepNewInputReadyAndLegacyModeUnchanged()
{
	auto &directory = *mDirectory;
	KImageAnnotator annotator;
	QVERIFY(!annotator.rasterBackingEnabled());
	QVERIFY(annotator.enableRasterBacking(directory.path()));
	QVERIFY(annotator.rasterBackingEnabled());
	QVERIFY(annotator.canAcceptImage());
	annotator.loadImage(pattern(1));
	QVERIFY(annotator.isCurrentImageReady());
	QVERIFY(!annotator.image().isNull());
	QTRY_VERIFY(annotator.canAcceptImage());
	annotator.insertImageItem(QPointF(2, 3), pattern(8, 1.0, QSize(11, 13)));
	QVERIFY(annotator.isCurrentImageReady());
	QVERIFY(!annotator.image().isNull());
	QTRY_VERIFY(annotator.canAcceptImage());
	annotator.hide();
	QVERIFY(annotator.isCurrentImageReady());
	for (const auto &handle : annotator.findChild<AnnotationTabWidget *>()->currentAnnotationArea()->rasterDocument()->handles()) {
		QVERIFY(!handle->image.isNull());
	}
	QSignalSpy readiness(&annotator, &KImageAnnotator::currentImageReadyChanged);
	QCOMPARE(annotator.addTab(pattern(2), QString(), QString()), 1);
	QVERIFY(annotator.isCurrentImageReady());
	QVERIFY(!readiness.isEmpty());
	QVERIFY(readiness.last().first().toBool());
	QTRY_VERIFY(annotator.canAcceptImage());
	annotator.beginTabBatch();
	annotator.removeTab(1);
	annotator.removeTab(0);
	annotator.endTabBatch();
	QTRY_VERIFY(annotator.canAcceptImage());
	KImageAnnotator legacy;
	legacy.addTab(pattern(1), QString(), QString());
	legacy.addTab(pattern(2), QString(), QString());
	QVERIFY(!legacy.rasterBackingEnabled());
	QVERIFY(!legacy.imageAt(0).isNull());
	QVERIFY(!legacy.imageAt(1).isNull());
	QVERIFY(!legacy.enableRasterBacking(directory.path()));
	legacy.removeTab(1);
	legacy.removeTab(0);
	QVERIFY(!legacy.enableRasterBacking(directory.path()));
}

void RasterPagingTest::Shutdown_Should_RemoveSessionAfterDrainingWorkerAndCancelingRequests()
{
	MockDefaultParameters parameters;
	initSettings(parameters.settingsProvider);
	AnnotationTabWidget tabs(&parameters.config, &parameters.settingsProvider);
	QVERIFY(tabs.enableRasterBacking(mDirectory->path()));
	tabs.addTab(pattern(1), QString(), QString());
	auto store = tabs.findChild<RasterStore *>();
	auto session = store->sessionDirectory();
	QSignalSpy closed(&tabs, &AnnotationTabWidget::rasterBackingClosed);
	QSignalSpy results(&tabs, &AnnotationTabWidget::imageRequestFinished);
	tabs.requestImageAt(0, 42);
	QCoreApplication::processEvents();
	QVERIFY(store->isBusy());
	tabs.closeRasterBacking();
	QCOMPARE(closed.count(), 0);
	QVERIFY(!tabs.canAcceptImage());
	QVERIFY(!tabs.isCurrentImageReady());
	QTRY_COMPARE(results.count(), 1);
	QVERIFY(results.first().at(2).toBool());
	QTRY_COMPARE(closed.count(), 1);
	QVERIFY(!QFileInfo::exists(session));
	QCOMPARE(tabs.count(), 0);
}

TEST_MAIN(RasterPagingTest);
