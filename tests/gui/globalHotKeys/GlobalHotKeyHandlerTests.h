/*
 * Copyright (C) 2026 Damir Porobic <damir.porobic@gmx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef KSNIP_GLOBALHOTKEYHANDLERTESTS_H
#define KSNIP_GLOBALHOTKEYHANDLERTESTS_H

#include <QtTest>

class GlobalHotKeyHandlerTests : public QObject
{
	Q_OBJECT
private slots:
	void CaptureModeForShortcutId_Should_MapBuiltInCaptureIds();
	void ShortcutId_Should_IdentifyOnlyOcrAction();
};

#endif // KSNIP_GLOBALHOTKEYHANDLERTESTS_H
