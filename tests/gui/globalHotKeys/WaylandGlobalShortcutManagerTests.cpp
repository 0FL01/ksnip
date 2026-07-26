/*
 * Copyright (C) 2026 Damir Porobic <damir.porobic@gmx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "WaylandGlobalShortcutManagerTests.h"

#include "src/gui/globalHotKeys/WaylandGlobalShortcutManager.h"
#include "tests/utils/TestRunner.h"

void WaylandGlobalShortcutManagerTests::PreferredTrigger_Should_ConvertSupportedSingleChord()
{
	QCOMPARE(WaylandGlobalShortcutManager::preferredTrigger(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_R)), QLatin1String("ALT+SHIFT+r"));
	QCOMPARE(WaylandGlobalShortcutManager::preferredTrigger(QKeySequence(Qt::META | Qt::Key_F1)), QLatin1String("LOGO+F1"));
}

void WaylandGlobalShortcutManagerTests::PreferredTrigger_Should_ReturnEmpty_When_SequenceIsUnsafe()
{
	QVERIFY(WaylandGlobalShortcutManager::preferredTrigger(QKeySequence(QLatin1String("Ctrl+A, Ctrl+B"))).isEmpty());
	QVERIFY(WaylandGlobalShortcutManager::preferredTrigger(QKeySequence(Qt::CTRL)).isEmpty());
}

void WaylandGlobalShortcutManagerTests::RegisterDBusTypes_Should_RegisterShortcutListSignature()
{
	WaylandGlobalShortcutManager::registerDBusTypes();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(QMetaType::fromType<PortalShortcutList>())), QLatin1String("a(sa{sv})"));
#else
	QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(qMetaTypeId<PortalShortcutList>())), QLatin1String("a(sa{sv})"));
#endif
}

TEST_MAIN(WaylandGlobalShortcutManagerTests)
