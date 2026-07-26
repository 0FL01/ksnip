/*
 * Copyright (C) 2026 Damir Porobic <damir.porobic@gmx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef KSNIP_WAYLANDGLOBALSHORTCUTMANAGERTESTS_H
#define KSNIP_WAYLANDGLOBALSHORTCUTMANAGERTESTS_H

#include <QtTest>

class WaylandGlobalShortcutManagerTests : public QObject
{
	Q_OBJECT
private slots:
	void PreferredTrigger_Should_ConvertSupportedSingleChord();
	void PreferredTrigger_Should_ReturnEmpty_When_SequenceIsUnsafe();
	void RegisterDBusTypes_Should_RegisterShortcutListSignature();
};

#endif // KSNIP_WAYLANDGLOBALSHORTCUTMANAGERTESTS_H
