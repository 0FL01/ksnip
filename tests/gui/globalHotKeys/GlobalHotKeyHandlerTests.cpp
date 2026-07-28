/*
 * Copyright (C) 2026 Damir Porobic <damir.porobic@gmx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "GlobalHotKeyHandlerTests.h"

#include "src/gui/globalHotKeys/GlobalHotKeyHandler.h"
#include "tests/utils/TestRunner.h"

void GlobalHotKeyHandlerTests::CaptureModeForShortcutId_Should_MapBuiltInCaptureIds()
{
	const QList<QPair<QString, CaptureModes>> mappings{
		{ QLatin1String("capture.rect_area"), CaptureModes::RectArea },
		{ QLatin1String("capture.full_screen"), CaptureModes::FullScreen },
		{ QLatin1String("capture.current_screen"), CaptureModes::CurrentScreen },
		{ QLatin1String("capture.active_window"), CaptureModes::ActiveWindow },
		{ QLatin1String("capture.select_window"), CaptureModes::WindowUnderCursor }
	};
	for(const auto &mapping : mappings) {
		CaptureModes captureMode;
		QVERIFY(GlobalHotKeyHandler::captureModeForShortcutId(mapping.first, &captureMode));
		QCOMPARE(captureMode, mapping.second);
	}

	CaptureModes captureMode;
	QVERIFY(!GlobalHotKeyHandler::captureModeForShortcutId(QLatin1String("unknown"), &captureMode));
}

void GlobalHotKeyHandlerTests::ShortcutId_Should_IdentifyOnlyOcrAction()
{
	QVERIFY(GlobalHotKeyHandler::isOcrShortcutId(QLatin1String("ocr.rect_area")));
	QVERIFY(!GlobalHotKeyHandler::isOcrShortcutId(QLatin1String("capture.rect_area")));
	QVERIFY(!GlobalHotKeyHandler::isOcrShortcutId(QLatin1String("unknown")));
}

TEST_MAIN(GlobalHotKeyHandlerTests)
