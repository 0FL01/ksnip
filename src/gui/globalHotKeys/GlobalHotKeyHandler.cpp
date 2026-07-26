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

#include "GlobalHotKeyHandler.h"

GlobalHotKeyHandler::GlobalHotKeyHandler(
		const QList<CaptureModes> &supportedCaptureModes,
		const QSharedPointer<IPlatformChecker> &platformChecker,
		const QSharedPointer<IConfig> &config,
		QObject *parent) :
	QObject(parent),
	mConfig(config),
	mSupportedCaptureModes(supportedCaptureModes),
	mPlatformChecker(platformChecker),
#ifdef Q_OS_LINUX
	mWaylandShortcutManager(nullptr),
#endif
	mEnabled(true),
	mHotKeysDirty(true)
{
#ifdef Q_OS_LINUX
	if(mPlatformChecker->isWayland()) {
		mWaylandShortcutManager = new WaylandGlobalShortcutManager(this);
		connect(mWaylandShortcutManager, &WaylandGlobalShortcutManager::activated, this, &GlobalHotKeyHandler::captureTriggered);
		connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, mWaylandShortcutManager, &WaylandGlobalShortcutManager::stop);
	}
#endif
	connect(mConfig.data(), &IConfig::hotKeysChanged, this, &GlobalHotKeyHandler::hotKeysChanged);

	setupHotKeys();
}

GlobalHotKeyHandler::~GlobalHotKeyHandler()
{
	removeHotKeys();
}

void GlobalHotKeyHandler::removeHotKeys()
{
	mGlobalHotKeys.clear();
#ifdef Q_OS_LINUX
	if(mWaylandShortcutManager != nullptr) {
		mWaylandShortcutManager->stop();
	}
#endif
}

void GlobalHotKeyHandler::setupHotKeys()
{
	mHotKeysDirty = false;
	removeHotKeys();
	if(!mEnabled || !mConfig->globalHotKeysEnabled()) {
		return;
	}

#ifdef Q_OS_LINUX
	if(mWaylandShortcutManager != nullptr) {
		setupWaylandHotKeys();
		return;
	}
#endif

	createHotKey(mConfig->rectAreaHotKey(), CaptureModes::RectArea);
	createHotKey(mConfig->lastRectAreaHotKey(), CaptureModes::LastRectArea);
	createHotKey(mConfig->fullScreenHotKey(), CaptureModes::FullScreen);
	createHotKey(mConfig->currentScreenHotKey(), CaptureModes::CurrentScreen);
	createHotKey(mConfig->activeWindowHotKey(), CaptureModes::ActiveWindow);
	createHotKey(mConfig->windowUnderCursorHotKey(), CaptureModes::WindowUnderCursor);
	createHotKey(mConfig->portalHotKey(), CaptureModes::Portal);

	auto actions = mConfig->actions();
	for (const auto& action : actions) {
		createHotKey(action);
	}
}

void GlobalHotKeyHandler::hotKeysChanged()
{
	mHotKeysDirty = true;
	if(mEnabled) {
		setupHotKeys();
	}
}

void GlobalHotKeyHandler::createHotKey(const QKeySequence &keySequence, CaptureModes captureMode)
{
	if(mSupportedCaptureModes.contains(captureMode) && !keySequence.isEmpty()) {
		auto hotKey = QSharedPointer<GlobalHotKey>(new GlobalHotKey(QApplication::instance(), keySequence, mPlatformChecker));
		connect(hotKey.data(), &GlobalHotKey::pressed, [this, captureMode](){ emit captureTriggered(captureMode); });
		mGlobalHotKeys.append(hotKey);
	}
}

void GlobalHotKeyHandler::createHotKey(const Action &action)
{
	auto isGlobal = action.isGlobalShortcut();
	auto isShortcutSet = !action.shortcut().isEmpty();
	auto isPostProcessingOnlyAction = !action.isCaptureEnabled();
	auto isRequestedCaptureSupported = action.isCaptureEnabled() && mSupportedCaptureModes.contains(action.captureMode());
	if(isShortcutSet && isGlobal && (isPostProcessingOnlyAction || isRequestedCaptureSupported)) {
		auto hotKey = QSharedPointer<GlobalHotKey>(new GlobalHotKey(QApplication::instance(), action.shortcut(), mPlatformChecker));
		connect(hotKey.data(), &GlobalHotKey::pressed, [this, action](){ emit actionTriggered(action); });
		mGlobalHotKeys.append(hotKey);
	}
}

void GlobalHotKeyHandler::setEnabled(bool enabled)
{
	if(mEnabled == enabled) {
		return;
	}

	mEnabled = enabled;
	if(!enabled) {
		mHotKeysDirty = true;
		removeHotKeys();
	} else if(mHotKeysDirty) {
		setupHotKeys();
	}
}

void GlobalHotKeyHandler::configureWaylandShortcuts()
{
#ifdef Q_OS_LINUX
	if(mWaylandShortcutManager != nullptr) {
		mWaylandShortcutManager->requestConfigureShortcuts();
	}
#endif
}

#ifdef Q_OS_LINUX
void GlobalHotKeyHandler::setupWaylandHotKeys()
{
	QList<WaylandGlobalShortcutManager::Shortcut> shortcuts;
	addWaylandShortcut(shortcuts, QLatin1String("capture.rect_area"), tr("Capture rectangular area"), mConfig->rectAreaHotKey(), CaptureModes::RectArea);
	addWaylandShortcut(shortcuts, QLatin1String("capture.full_screen"), tr("Capture full screen"), mConfig->fullScreenHotKey(), CaptureModes::FullScreen);
	addWaylandShortcut(shortcuts, QLatin1String("capture.current_screen"), tr("Capture current screen"), mConfig->currentScreenHotKey(), CaptureModes::CurrentScreen);
	addWaylandShortcut(shortcuts, QLatin1String("capture.active_window"), tr("Capture active window"), mConfig->activeWindowHotKey(), CaptureModes::ActiveWindow);
	addWaylandShortcut(shortcuts, QLatin1String("capture.select_window"), tr("Select window to capture"), mConfig->windowUnderCursorHotKey(), CaptureModes::WindowUnderCursor);
	mWaylandShortcutManager->start(shortcuts);
}

void GlobalHotKeyHandler::addWaylandShortcut(QList<WaylandGlobalShortcutManager::Shortcut> &shortcuts,
													 const QString &id,
													 const QString &description,
													 const QKeySequence &keySequence,
													 CaptureModes captureMode) const
{
	if(mSupportedCaptureModes.contains(captureMode)) {
		shortcuts.append({ id, description, keySequence, captureMode });
	}
}
#endif
