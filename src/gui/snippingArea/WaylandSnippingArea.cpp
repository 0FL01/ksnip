/*
 * Copyright (C) 2021 Damir Porobic <damir.porobic@gmx.com>
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

#include "WaylandSnippingArea.h"

WaylandSnippingArea::WaylandSnippingArea(const QSharedPointer<IConfig> &config) : X11SnippingArea(config)
{
}

QRect WaylandSnippingArea::selectedRectArea() const
{
	return mHdpiScaler.scale(getCaptureArea());
}

QRect WaylandSnippingArea::selectedLogicalRectArea() const
{
	return getGlobalCaptureArea();
}

QRect WaylandSnippingArea::selectedRectAreaForBackground(const QSize &backgroundPixelSize) const
{
	auto captureArea = getCaptureArea();
	auto canvasSize = getGeometry().size();
	if (!captureArea.isValid() || backgroundPixelSize.isEmpty() || canvasSize.isEmpty()) {
		return {};
	}

	auto scaleX = backgroundPixelSize.width() / canvasSize.width();
	auto scaleY = backgroundPixelSize.height() / canvasSize.height();
	auto left = qBound(qint64 { 0 },
					   qRound64(captureArea.x() * scaleX),
					   static_cast<qint64>(backgroundPixelSize.width()));
	auto right = qBound(qint64 { 0 },
						qRound64((captureArea.x() + captureArea.width()) * scaleX),
						static_cast<qint64>(backgroundPixelSize.width()));
	auto top = qBound(qint64 { 0 },
					  qRound64(captureArea.y() * scaleY),
					  static_cast<qint64>(backgroundPixelSize.height()));
	auto bottom = qBound(qint64 { 0 },
						 qRound64((captureArea.y() + captureArea.height()) * scaleY),
						 static_cast<qint64>(backgroundPixelSize.height()));
	return { static_cast<int>(left),
			 static_cast<int>(top),
			 static_cast<int>(right - left),
			 static_cast<int>(bottom - top) };
}

void WaylandSnippingArea::grabKeyboardFocus()
{
	QApplication::setActiveWindow(this);
	setFocus();
	grabKeyboard();
}
