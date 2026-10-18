/*
 * Copyright (C) 2020 Damir Porobic <damir.porobic@gmx.com>
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

#include "CaptureTabStateHandler.h"

CaptureTabStateHandler::CaptureTabStateHandler() :
	mCurrentTabIndex(-1),
	mNextDocumentId(1)
{
}

void CaptureTabStateHandler::add(int index, const QString &filename, const QString &path, bool isSaved)
{
	auto newState = QSharedPointer<CaptureTabState>(new CaptureTabState(index, mNextDocumentId++, filename, path, isSaved));
	mCaptureTabStates.append(newState);
	refreshTabInfo(index);
}

bool CaptureTabStateHandler::isSaved(int index)
{
	auto tabState = getTabState(index);
	return tabState.isNull() || tabState->isSaved;
}

bool CaptureTabStateHandler::isPathValid(int index)
{
	return PathHelper::isPathValid(path(index));
}

QString CaptureTabStateHandler::path(int index)
{
	auto tabState = getTabState(index);
	return tabState.isNull() ? QString() : tabState->path;
}

QString CaptureTabStateHandler::filename(int index)
{
	auto tabState = getTabState(index);
	return tabState.isNull() ? QString() : tabState->filename;
}

void CaptureTabStateHandler::setSaveState(int index, const SaveResultDto &saveResult)
{
	auto tabState = getTabState(index);
	if (!tabState.isNull()) {
		tabState->isSaved = saveResult.isSuccessful;

		if (saveResult.isSuccessful) {
			tabState->path = saveResult.path;
			tabState->filename = PathHelper::extractFilename(saveResult.path);
			refreshTabInfo(index);
		}
	}
}

void CaptureTabStateHandler::renameFile(int index, const RenameResultDto &renameResult)
{
	auto tabState = getTabState(index);
	if (!tabState.isNull() && renameResult.isSuccessful) {
		tabState->path = renameResult.path;
		tabState->filename = PathHelper::extractFilename(renameResult.path);
		refreshTabInfo(index);
	}
}

void CaptureTabStateHandler::tabMoved(int fromIndex, int toIndex)
{
	for (auto& state : mCaptureTabStates) {
		if(state->index == fromIndex) {
			state->index = toIndex;
		} else if (fromIndex < toIndex && state->index > fromIndex && state->index <= toIndex) {
			--state->index;
		} else if (fromIndex > toIndex && state->index >= toIndex && state->index < fromIndex) {
			++state->index;
		}
	}
	if (mCurrentTabIndex == fromIndex) {
		mCurrentTabIndex = toIndex;
	} else if (fromIndex < toIndex && mCurrentTabIndex > fromIndex && mCurrentTabIndex <= toIndex) {
		--mCurrentTabIndex;
	} else if (fromIndex > toIndex && mCurrentTabIndex >= toIndex && mCurrentTabIndex < fromIndex) {
		++mCurrentTabIndex;
	}
}

void CaptureTabStateHandler::currentTabChanged(int index)
{
	mCurrentTabIndex = index;
}

void CaptureTabStateHandler::tabRemoved(int index)
{
	auto iterator = mCaptureTabStates.begin();
	while (iterator != mCaptureTabStates.end()) {
		if (iterator->data()->index == index) {
			iterator = mCaptureTabStates.erase(iterator);
		} else {
			if (iterator->data()->index > index) {
				iterator->data()->index--;
			}
			iterator++;
		}
	}
}

void CaptureTabStateHandler::currentTabContentChanged()
{
	auto currentTabState = getTabState(mCurrentTabIndex);
	if (!currentTabState.isNull()) {
		++currentTabState->revision;
		currentTabState->isSaved = false;
		refreshTabInfo(mCurrentTabIndex);
	}
}

void CaptureTabStateHandler::refreshTabInfo(int index)
{
	auto tabState = getTabState(index);
	if (!tabState.isNull()) {
		auto title = tabState->isSaved ? tabState->filename : tabState->filename + QLatin1String("*");
		emit updateTabInfo(tabState->index, title, tabState->path);
	}
}

int CaptureTabStateHandler::count() const
{
	return mCaptureTabStates.count();
}

int CaptureTabStateHandler::currentTabIndex() const
{
	return mCurrentTabIndex;
}

quint64 CaptureTabStateHandler::documentId(int index) const
{
	auto state = getTabState(index);
	return state.isNull() ? 0 : state->documentId;
}

quint64 CaptureTabStateHandler::revision(int index) const
{
	auto state = getTabState(index);
	return state.isNull() ? 0 : state->revision;
}

int CaptureTabStateHandler::indexForDocument(quint64 documentId) const
{
	for (const auto &state : mCaptureTabStates) {
		if (state->documentId == documentId) {
			return state->index;
		}
	}
	return -1;
}

bool CaptureTabStateHandler::setSaveStateIfRevision(quint64 documentId, quint64 revision, const SaveResultDto &saveResult)
{
	auto index = indexForDocument(documentId);
	auto state = getTabState(index);
	if (state.isNull() || state->revision != revision || !saveResult.isSuccessful) {
		return false;
	}
	setSaveState(index, saveResult);
	return indexForDocument(documentId) >= 0 && state->revision == revision && state->isSaved;
}

QSharedPointer<CaptureTabState> CaptureTabStateHandler::getTabState(int index) const
{
	for (auto &state : mCaptureTabStates) {
		if (state->index == index) {
			return state;
		}
	}
	return QSharedPointer<CaptureTabState>(nullptr);
}
