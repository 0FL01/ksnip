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

#include "WaylandGlobalShortcutManager.h"

namespace
{
const auto PortalService = QLatin1String("org.freedesktop.portal.Desktop");
const auto PortalPath = QLatin1String("/org/freedesktop/portal/desktop");
const auto PortalInterface = QLatin1String("org.freedesktop.portal.GlobalShortcuts");
const auto PropertiesInterface = QLatin1String("org.freedesktop.DBus.Properties");
const auto RequestInterface = QLatin1String("org.freedesktop.portal.Request");
const auto SessionInterface = QLatin1String("org.freedesktop.portal.Session");
const auto ResponseSignal = QLatin1String("Response");
const auto ActivatedSignal = QLatin1String("Activated");
const auto HandleToken = QLatin1String("handle_token");
const auto SessionHandleToken = QLatin1String("session_handle_token");
const auto ShortcutsResult = QLatin1String("shortcuts");
const auto SessionHandleResult = QLatin1String("session_handle");
const auto RequestCleanupTimeoutMs = 5000;
const auto MethodReplyTimeoutMs = 4000;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcut &shortcut)
{
	argument.beginStructure();
	argument << shortcut.first << shortcut.second;
	argument.endStructure();
	return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcut &shortcut)
{
	argument.beginStructure();
	argument >> shortcut.first >> shortcut.second;
	argument.endStructure();
	return argument;
}

WaylandGlobalShortcutManager::WaylandGlobalShortcutManager(QObject *parent) :
	QObject(parent),
	mGeneration(0),
	mTokenCounter(0),
	mRequestCounter(0),
	mReconciled(false)
{
	registerDBusTypes();
	QDBusConnection::sessionBus().connect(PortalService,
										 PortalPath,
										 PortalInterface,
										 ActivatedSignal,
										 this,
										 SLOT(portalActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
}

WaylandGlobalShortcutManager::~WaylandGlobalShortcutManager()
{
	stop();
}

void WaylandGlobalShortcutManager::start(const QList<Shortcut> &shortcuts)
{
	stop();
	mShortcuts = shortcuts;
	for(const auto &shortcut : mShortcuts) {
		mDesiredIds.insert(shortcut.id);
	}

	if(!mShortcuts.isEmpty()) {
		probe(mGeneration);
	}
}

void WaylandGlobalShortcutManager::stop()
{
	mGeneration++;
	mReconciled = false;
	mBoundIds.clear();
	mDesiredIds.clear();
	mShortcuts.clear();

	if(!mCurrentRequestPath.isEmpty()) {
		auto request = mRequests.value(mCurrentRequestPath);
		closeRequest(mCurrentRequestPath, request.id);
		mCurrentRequestPath.clear();
	}

	if(!mSessionPath.isEmpty()) {
		closeSession(mSessionPath);
		mSessionPath.clear();
	}
}

void WaylandGlobalShortcutManager::registerDBusTypes()
{
	qDBusRegisterMetaType<PortalShortcut>();
	qDBusRegisterMetaType<PortalShortcutList>();
}

QString WaylandGlobalShortcutManager::preferredTrigger(const QKeySequence &keySequence)
{
	if(keySequence.count() != 1) {
		return {};
	}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	auto combined = keySequence[0].toCombined();
#else
	auto combined = keySequence[0];
#endif
	auto modifiers = Qt::KeyboardModifiers(combined & Qt::KeyboardModifierMask);
	if(modifiers.testFlag(Qt::GroupSwitchModifier) || modifiers.testFlag(Qt::KeypadModifier)) {
		return {};
	}

	auto key = combined & ~Qt::KeyboardModifierMask;
	auto name = keyName(key);
	if(name.isEmpty()) {
		return {};
	}

	QStringList trigger;
	if(modifiers.testFlag(Qt::ControlModifier)) {
		trigger.append(QLatin1String("CTRL"));
	}
	if(modifiers.testFlag(Qt::AltModifier)) {
		trigger.append(QLatin1String("ALT"));
	}
	if(modifiers.testFlag(Qt::ShiftModifier)) {
		trigger.append(QLatin1String("SHIFT"));
	}
	if(modifiers.testFlag(Qt::MetaModifier)) {
		trigger.append(QLatin1String("LOGO"));
	}
	trigger.append(name);
	return trigger.join(QLatin1Char('+'));
}

QString WaylandGlobalShortcutManager::keyName(int key)
{
	if(key >= Qt::Key_A && key <= Qt::Key_Z) {
		return QChar(QLatin1Char('a').unicode() + key - Qt::Key_A);
	}
	if(key >= Qt::Key_0 && key <= Qt::Key_9) {
		return QChar(QLatin1Char('0').unicode() + key - Qt::Key_0);
	}
	if(key >= Qt::Key_F1 && key <= Qt::Key_F12) {
		return QStringLiteral("F%1").arg(key - Qt::Key_F1 + 1);
	}

	static const QHash<int, QString> names = {
		{ Qt::Key_Escape, QLatin1String("Escape") },
		{ Qt::Key_Backspace, QLatin1String("BackSpace") },
		{ Qt::Key_Return, QLatin1String("Return") },
		{ Qt::Key_Insert, QLatin1String("Insert") },
		{ Qt::Key_Delete, QLatin1String("Delete") },
		{ Qt::Key_Pause, QLatin1String("Pause") },
		{ Qt::Key_Print, QLatin1String("Print") },
		{ Qt::Key_Home, QLatin1String("Home") },
		{ Qt::Key_End, QLatin1String("End") },
		{ Qt::Key_Left, QLatin1String("Left") },
		{ Qt::Key_Up, QLatin1String("Up") },
		{ Qt::Key_Right, QLatin1String("Right") },
		{ Qt::Key_Down, QLatin1String("Down") },
		{ Qt::Key_PageUp, QLatin1String("Prior") },
		{ Qt::Key_PageDown, QLatin1String("Next") },
		{ Qt::Key_Comma, QLatin1String("comma") },
		{ Qt::Key_Underscore, QLatin1String("underscore") },
		{ Qt::Key_Minus, QLatin1String("minus") },
		{ Qt::Key_Period, QLatin1String("period") },
		{ Qt::Key_Slash, QLatin1String("slash") },
		{ Qt::Key_Colon, QLatin1String("colon") },
		{ Qt::Key_Semicolon, QLatin1String("semicolon") }
	};
	return names.value(key);
}

QString WaylandGlobalShortcutManager::createToken(const QString &prefix)
{
	mTokenCounter++;
	return QStringLiteral("ksnip_%1_%2").arg(prefix).arg(mTokenCounter);
}

QString WaylandGlobalShortcutManager::predictedRequestPath(const QString &token) const
{
	auto sender = QDBusConnection::sessionBus().baseService();
	sender.remove(0, 1);
	sender.replace(QLatin1Char('.'), QLatin1Char('_'));
	return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
}

void WaylandGlobalShortcutManager::probe(quint64 generation)
{
	auto message = QDBusMessage::createMethodCall(PortalService, PortalPath, PropertiesInterface, QLatin1String("Get"));
	message << PortalInterface << QLatin1String("version");
	auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, MethodReplyTimeoutMs), this);
	connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *watcher) {
		QDBusPendingReply<QDBusVariant> reply = *watcher;
		watcher->deleteLater();
		if(generation != mGeneration) {
			return;
		}
		if(reply.isError()) {
			fail(tr("GlobalShortcuts portal probe failed: %1").arg(reply.error().message()));
			return;
		}

		auto value = reply.value().variant();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		auto isUnsignedInteger = value.metaType().id() == QMetaType::UInt;
#else
		auto isUnsignedInteger = value.type() == QVariant::UInt;
#endif
		if(!isUnsignedInteger || value.toUInt() < 1) {
			fail(tr("GlobalShortcuts portal returned an unsupported version"));
			return;
		}
		createSession(generation);
	});
}

void WaylandGlobalShortcutManager::createSession(quint64 generation)
{
	auto token = createToken(QLatin1String("create"));
	QVariantMap options{
		{ HandleToken, token },
		{ SessionHandleToken, createToken(QLatin1String("session")) }
	};
	beginRequest(RequestKind::Create, generation, QLatin1String("CreateSession"), { options }, token);
}

void WaylandGlobalShortcutManager::listShortcuts(quint64 generation)
{
	auto token = createToken(QLatin1String("list"));
	QVariantMap options{ { HandleToken, token } };
	beginRequest(RequestKind::List,
				 generation,
				 QLatin1String("ListShortcuts"),
				 { QVariant::fromValue(QDBusObjectPath(mSessionPath)), options },
				 token);
}

void WaylandGlobalShortcutManager::bindShortcuts(quint64 generation)
{
	auto token = createToken(QLatin1String("bind"));
	QVariantMap options{ { HandleToken, token } };
	beginRequest(RequestKind::Bind,
				 generation,
				 QLatin1String("BindShortcuts"),
				 { QVariant::fromValue(QDBusObjectPath(mSessionPath)),
				   QVariant::fromValue(portalShortcuts(mShortcuts)),
				   QString(),
				   options },
				 token);
}

void WaylandGlobalShortcutManager::beginRequest(RequestKind kind,
												quint64 generation,
												const QString &method,
												const QVariantList &arguments,
												const QString &token)
{
	if(generation != mGeneration || !mCurrentRequestPath.isEmpty()) {
		return;
	}

	auto path = predictedRequestPath(token);
	auto requestId = ++mRequestCounter;
	mRequests.insert(path, { kind, generation, requestId });
	mCurrentRequestPath = path;
	QDBusConnection::sessionBus().connect(QString(), path, RequestInterface, ResponseSignal, this, SLOT(portalResponse(uint,QVariantMap)));

	auto message = QDBusMessage::createMethodCall(PortalService, PortalPath, PortalInterface, method);
	message.setArguments(arguments);
	auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, MethodReplyTimeoutMs), this);
	connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, path, requestId](QDBusPendingCallWatcher *watcher) {
		processRequestReply(path, requestId, watcher);
	});
}

void WaylandGlobalShortcutManager::processRequestReply(const QString &expectedPath, quint64 requestId, QDBusPendingCallWatcher *watcher)
{
	QDBusPendingReply<QDBusObjectPath> reply = *watcher;
	watcher->deleteLater();
	auto request = mRequests.find(expectedPath);
	if(request == mRequests.end() || request->id != requestId) {
		return;
	}

	if(reply.isError()) {
		auto generation = request->generation;
		QDBusConnection::sessionBus().disconnect(QString(), expectedPath, RequestInterface, ResponseSignal, this, SLOT(portalResponse(uint,QVariantMap)));
		mRequests.erase(request);
		if(mCurrentRequestPath == expectedPath) {
			mCurrentRequestPath.clear();
		}
		if(generation == mGeneration) {
			fail(tr("GlobalShortcuts portal request failed: %1").arg(reply.error().message()));
		}
		return;
	}

	auto actualPath = reply.value().path();
	if(actualPath.isEmpty() || actualPath == expectedPath) {
		return;
	}

	auto requestValue = request.value();
	mRequests.erase(request);
	mRequests.insert(actualPath, requestValue);
	QDBusConnection::sessionBus().connect(QString(), actualPath, RequestInterface, ResponseSignal, this, SLOT(portalResponse(uint,QVariantMap)));
	QDBusConnection::sessionBus().disconnect(QString(), expectedPath, RequestInterface, ResponseSignal, this, SLOT(portalResponse(uint,QVariantMap)));
	if(mCurrentRequestPath == expectedPath) {
		mCurrentRequestPath = actualPath;
	}
	if(requestValue.generation != mGeneration) {
		closeRequest(actualPath, requestId);
	}
}

void WaylandGlobalShortcutManager::portalResponse(uint response, const QVariantMap &results)
{
	auto path = message().path();
	auto request = mRequests.find(path);
	if(request == mRequests.end()) {
		return;
	}

	auto requestValue = request.value();
	QDBusConnection::sessionBus().disconnect(QString(), path, RequestInterface, ResponseSignal, this, SLOT(portalResponse(uint,QVariantMap)));
	mRequests.erase(request);
	auto isCurrent = requestValue.generation == mGeneration && mCurrentRequestPath == path;
	if(isCurrent) {
		mCurrentRequestPath.clear();
	}

	if(!isCurrent) {
		if(requestValue.kind == RequestKind::Create && response == 0) {
			closeSession(results.value(SessionHandleResult).toString());
		}
		return;
	}

	if(response != 0) {
		fail(tr("GlobalShortcuts portal request was canceled or denied"));
		return;
	}

	switch(requestValue.kind) {
	case RequestKind::Create:
		processCreateResponse(results, requestValue.generation);
		break;
	case RequestKind::List:
		processListResponse(results, requestValue.generation);
		break;
	case RequestKind::Bind:
		processBindResponse(results, requestValue.generation);
		break;
	}
}

void WaylandGlobalShortcutManager::processCreateResponse(const QVariantMap &results, quint64 generation)
{
	auto path = results.value(SessionHandleResult).toString();
	QDBusObjectPath session(path);
	if(path.isEmpty() || session.path() != path) {
		fail(tr("GlobalShortcuts portal returned an invalid session handle"));
		return;
	}

	mSessionPath = path;
	listShortcuts(generation);
}

void WaylandGlobalShortcutManager::processListResponse(const QVariantMap &results, quint64 generation)
{
	bool ok = false;
	auto existingIds = shortcutIds(results, &ok);
	if(!ok) {
		fail(tr("GlobalShortcuts portal returned an invalid shortcut list"));
		return;
	}

	if(existingIds == mDesiredIds) {
		setActive(existingIds);
	} else {
		bindShortcuts(generation);
	}
}

void WaylandGlobalShortcutManager::processBindResponse(const QVariantMap &results, quint64 generation)
{
	Q_UNUSED(generation)
	bool ok = false;
	auto boundIds = shortcutIds(results, &ok);
	if(!ok) {
		fail(tr("GlobalShortcuts portal returned an invalid bound shortcut list"));
		return;
	}
	setActive(boundIds);
}

void WaylandGlobalShortcutManager::setActive(const QSet<QString> &boundIds)
{
	mBoundIds = boundIds;
	mReconciled = true;
}

void WaylandGlobalShortcutManager::fail(const QString &error)
{
	qWarning("%s", qPrintable(error));
	stop();
}

void WaylandGlobalShortcutManager::closeRequest(const QString &path, quint64 requestId)
{
	auto message = QDBusMessage::createMethodCall(PortalService, path, RequestInterface, QLatin1String("Close"));
	QDBusConnection::sessionBus().asyncCall(message);
	QTimer::singleShot(RequestCleanupTimeoutMs, this, [this, path, requestId]() {
		auto request = mRequests.find(path);
		if(request != mRequests.end() && request->id == requestId) {
			QDBusConnection::sessionBus().disconnect(QString(), path, RequestInterface, ResponseSignal, this, SLOT(portalResponse(uint,QVariantMap)));
			mRequests.erase(request);
		}
	});
}

void WaylandGlobalShortcutManager::closeSession(const QString &path)
{
	if(path.isEmpty()) {
		return;
	}
	auto message = QDBusMessage::createMethodCall(PortalService, path, SessionInterface, QLatin1String("Close"));
	QDBusConnection::sessionBus().asyncCall(message);
}

QSet<QString> WaylandGlobalShortcutManager::shortcutIds(const QVariantMap &results, bool *ok)
{
	*ok = false;
	if(!results.contains(ShortcutsResult)) {
		return {};
	}

	auto shortcuts = qdbus_cast<PortalShortcutList>(results.value(ShortcutsResult));
	QSet<QString> ids;
	for(const auto &shortcut : shortcuts) {
		if(shortcut.first.isEmpty()) {
			return {};
		}
		ids.insert(shortcut.first);
	}
	*ok = true;
	return ids;
}

PortalShortcutList WaylandGlobalShortcutManager::portalShortcuts(const QList<Shortcut> &shortcuts)
{
	PortalShortcutList result;
	for(const auto &shortcut : shortcuts) {
		QVariantMap properties{ { QLatin1String("description"), shortcut.description } };
		auto trigger = preferredTrigger(shortcut.keySequence);
		if(!trigger.isEmpty()) {
			properties.insert(QLatin1String("preferred_trigger"), trigger);
		}
		result.append(qMakePair(shortcut.id, properties));
	}
	return result;
}

void WaylandGlobalShortcutManager::portalActivated(const QDBusObjectPath &sessionHandle,
															const QString &shortcutId,
															qulonglong timestamp,
															const QVariantMap &options)
{
	Q_UNUSED(timestamp)
	Q_UNUSED(options)
	if(!mReconciled || sessionHandle.path() != mSessionPath || !mBoundIds.contains(shortcutId)) {
		return;
	}

	for(const auto &shortcut : mShortcuts) {
		if(shortcut.id == shortcutId) {
			emit activated(shortcut.captureMode);
			return;
		}
	}
}
