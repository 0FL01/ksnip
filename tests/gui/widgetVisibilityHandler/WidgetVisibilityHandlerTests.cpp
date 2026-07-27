/*
 * Copyright (C) 2026 Damir Porobic <damir.porobic@gmx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "WidgetVisibilityHandlerTests.h"

#include "src/gui/widgetVisibilityHandler/WidgetVisibilityHandler.h"

#include "tests/utils/TestRunner.h"

void WidgetVisibilityHandlerTests::RestoreState_Should_RemainHidden_When_HiddenWasEnforced()
{
	// arrange
	QWidget widget;
	WidgetVisibilityHandler visibilityHandler(&widget);
	widget.show();
	QApplication::processEvents();

	// act
	visibilityHandler.makeInvisible();
	visibilityHandler.enforceHidden();
	visibilityHandler.restoreState();

	// assert
	QVERIFY(!widget.isVisible());
	QVERIFY(!widget.windowState().testFlag(Qt::WindowMinimized));
	QVERIFY(!widget.windowState().testFlag(Qt::WindowActive));
	QCOMPARE(widget.windowOpacity(), 1.0);
}

void WidgetVisibilityHandlerTests::EnforceVisible_Should_ShowWidget_When_HiddenWasEnforced()
{
	// arrange
	QWidget widget;
	WidgetVisibilityHandler visibilityHandler(&widget);
	widget.show();
	QApplication::processEvents();

	// act
	visibilityHandler.makeInvisible();
	visibilityHandler.enforceHidden();
	visibilityHandler.enforceVisible();
	QApplication::processEvents();

	// assert
	QVERIFY(widget.isVisible());
	QCOMPARE(widget.windowOpacity(), 1.0);
}

void WidgetVisibilityHandlerTests::RestoreState_Should_RestoreVisibleWidget_When_TemporarilyInvisible()
{
	// arrange
	QWidget widget;
	WidgetVisibilityHandler visibilityHandler(&widget);
	widget.show();
	QApplication::processEvents();

	// act
	visibilityHandler.makeInvisible();
	visibilityHandler.restoreState();
	QApplication::processEvents();

	// assert
	QVERIFY(widget.isVisible());
	QCOMPARE(widget.windowOpacity(), 1.0);
}

TEST_MAIN(WidgetVisibilityHandlerTests)
