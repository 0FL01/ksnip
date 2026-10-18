/*
 * Copyright (C) 2019 Damir Porobic <damir.porobic@gmx.com>
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

#ifndef KSNIP_CAPTURETABSTATE_H
#define KSNIP_CAPTURETABSTATE_H

#include <QString>
#include <QtGlobal>

struct CaptureTabState
{
	int index;
	quint64 documentId;
	quint64 revision;
	QString path;
	QString filename;
	bool isSaved;

	explicit CaptureTabState(int index, quint64 documentId, const QString &filename, const QString &path, bool isSave) {
		this->index = index;
		this->documentId = documentId;
		this->revision = 0;
		this->filename = filename;
		this->path = path;
		this->isSaved = isSave;
	}
};

#endif //KSNIP_CAPTURETABSTATE_H
