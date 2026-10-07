/*
 * Copyright (C) 2026 Damir Porobic <damir.porobic@gmx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "WaylandGlobalShortcutManagerTests.h"

#ifdef Q_OS_LINUX
namespace
{
const auto PortalService = QLatin1String("org.freedesktop.portal.Desktop");
const auto PortalPath = QLatin1String("/org/freedesktop/portal/desktop");
const auto PortalInterface = QLatin1String("org.freedesktop.portal.GlobalShortcuts");
const auto WaitTimeoutMs = 1500;
}

ShortcutPortalRequestFake::ShortcutPortalRequestFake(QObject *parent) : QObject(parent)
{
}

void ShortcutPortalRequestFake::Close()
{
	mCloseCount++;
}

ShortcutPortalSessionFake::ShortcutPortalSessionFake(QObject *parent) : QObject(parent)
{
}

void ShortcutPortalSessionFake::Close()
{
	mCloseCount++;
	mBound = false;
	mBoundIds.clear();
}

ShortcutPortalFake::~ShortcutPortalFake()
{
	if(mConnection) {
		if(mServiceRegistered) {
			mConnection->unregisterService(PortalService);
		}
		mConnection->unregisterObject(PortalPath, QDBusConnection::UnregisterTree);
		QDBusConnection::disconnectFromBus(mConnectionName);
	}
}

bool ShortcutPortalFake::start()
{
	// Check isolation before constructing any bus connection or acquiring a name.
	if(qgetenv("KSNIP_TEST_PRIVATE_DBUS") != "1" || qgetenv("DBUS_SESSION_BUS_ADDRESS").isEmpty()) {
		mError = QStringLiteral("Run this test through its dbus-run-session CTest wrapper");
		return false;
	}

	mConnectionName = QStringLiteral("ksnip-shortcut-test-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
	mConnection.reset(new QDBusConnection(QDBusConnection::connectToBus(
		QString::fromLocal8Bit(qgetenv("DBUS_SESSION_BUS_ADDRESS")), mConnectionName)));
	if(!mConnection->isConnected()) {
		mError = mConnection->lastError().message();
		return false;
	}
	if(!registerObject(PortalPath, this)) {
		return false;
	}
	mServiceRegistered = mConnection->registerService(PortalService);
	if(!mServiceRegistered) {
		mError = mConnection->lastError().message();
	}
	return mServiceRegistered;
}

uint ShortcutPortalFake::version() const
{
	return mVersion;
}

bool ShortcutPortalFake::registerObject(const QString &path, QObject *object)
{
	if(mConnection->registerObject(path, object, QDBusConnection::ExportAllSlots |
												 QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties)) {
		return true;
	}
	mError = QStringLiteral("Cannot register %1: %2").arg(path, mConnection->lastError().message());
	QTest::qFail(qPrintable(mError), __FILE__, __LINE__);
	return false;
}

QString ShortcutPortalFake::handlePath(const QString &kind, const QString &token) const
{
	auto sender = message().service();
	sender.remove(0, 1);
	sender.replace(QLatin1Char('.'), QLatin1Char('_'));
	return QStringLiteral("/org/freedesktop/portal/desktop/%1/%2/%3").arg(kind, sender, token);
}

ShortcutPortalRequestFake *ShortcutPortalFake::createRequest(const QVariantMap &options, const QString &sessionPath)
{
	auto request = new ShortcutPortalRequestFake(this);
	request->mPath = handlePath(QLatin1String("request"), options.value(QLatin1String("handle_token")).toString());
	request->mSessionPath = sessionPath;
	registerObject(request->mPath, request);
	return request;
}

QDBusObjectPath ShortcutPortalFake::CreateSession(const QVariantMap &options)
{
	auto session = new ShortcutPortalSessionFake(this);
	session->mPath = handlePath(QLatin1String("session"), options.value(QLatin1String("session_handle_token")).toString());
	registerObject(session->mPath, session);
	mSessions.insert(session->mPath, session);
	auto request = createRequest(options, session->mPath);
	mCreateRequests.append(request);
	return QDBusObjectPath(request->mPath);
}

QDBusObjectPath ShortcutPortalFake::BindShortcuts(const QDBusObjectPath &session, const PortalShortcutList &shortcuts,
													const QString &parentWindow, const QVariantMap &options)
{
	Q_UNUSED(parentWindow)
	for(const auto previous : mBindRequests) {
		if(previous->mSessionPath == session.path()) {
			QTest::qFail("Only one BindShortcuts attempt is allowed per session", __FILE__, __LINE__);
		}
	}
	auto request = createRequest(options, session.path());
	request->mShortcuts = shortcuts;
	mBindRequests.append(request);
	if(mBindMethodError) {
		sendErrorReply(QLatin1String("org.freedesktop.portal.Error.Failed"), QLatin1String("Test BindShortcuts method error"));
	}
	return QDBusObjectPath(request->mPath);
}

QDBusObjectPath ShortcutPortalFake::ListShortcuts(const QDBusObjectPath &session, const QVariantMap &options)
{
	auto request = createRequest(options, session.path());
	mListRequests.append(request);
	// Trap the old reconciliation path: saved metadata does not bind a new session.
	QTimer::singleShot(0, request, [this, request]() {
		emit request->Response(0, { { QStringLiteral("shortcuts"), QVariant::fromValue(mSavedShortcuts) } });
	});
	return QDBusObjectPath(request->mPath);
}

void ShortcutPortalFake::ConfigureShortcuts(const QDBusObjectPath &session, const QString &parentWindow, const QVariantMap &options)
{
	Q_UNUSED(parentWindow)
	Q_UNUSED(options)
	mConfiguredSessions.append(session.path());
}

void ShortcutPortalFake::respondCreate(int index)
{
	auto request = mCreateRequests.at(index);
	emit request->Response(0, { { QStringLiteral("session_handle"), request->mSessionPath } });
}

void ShortcutPortalFake::respondBind(int index, const PortalShortcutList &shortcuts, uint response)
{
	auto request = mBindRequests.at(index);
	auto session = mSessions.value(request->mSessionPath);
	if(response == 0 && session && session->mCloseCount == 0) {
		session->mBound = true;
		for(const auto &shortcut : shortcuts) {
			session->mBoundIds.insert(shortcut.first);
		}
		mSavedShortcuts = shortcuts;
	}
	emit request->Response(response, { { QStringLiteral("shortcuts"), QVariant::fromValue(shortcuts) } });
}

bool ShortcutPortalFake::keypress(const QString &sessionPath, const QString &id)
{
	auto session = mSessions.value(sessionPath);
	if(!session || !session->mBound || session->mCloseCount != 0 || !session->mBoundIds.contains(id)) {
		return false;
	}
	rawActivated(sessionPath, id);
	return true;
}

void ShortcutPortalFake::rawActivated(const QString &sessionPath, const QString &id)
{
	emit Activated(QDBusObjectPath(sessionPath), id, qulonglong(1), QVariantMap());
}

void WaylandGlobalShortcutManagerTests::init()
{
	WaylandGlobalShortcutManager::registerDBusTypes();
	mPortal.reset(new ShortcutPortalFake);
	mPortal->mSavedShortcuts = shortcutMetadata();
	QVERIFY2(mPortal->start(), qPrintable(mPortal->mError));
}

void WaylandGlobalShortcutManagerTests::cleanup()
{
	if(mPortal && mPortal->mError.isEmpty()) {
		// Local managers have stopped; deliver their asynchronous Close calls before
		// releasing the fake service and its exported objects.
		QVERIFY(drainClientCalls());
		QDBusConnection::sessionBus().disconnect(PortalService, PortalPath, PortalInterface, QLatin1String("Activated"),
			this, SLOT(observeActivation(QDBusObjectPath,QString,qulonglong,QVariantMap)));
	}
	mPortal.reset();
}

QList<WaylandGlobalShortcutManager::Shortcut> WaylandGlobalShortcutManagerTests::shortcuts()
{
	// Keep OCR explicit so this manager-level test also runs in non-OCR test builds.
	return {
		{ QStringLiteral("capture.rect_area"), QStringLiteral("Rectangular area"), QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_R) },
		{ QStringLiteral("capture.full_screen"), QStringLiteral("Full screen"), QKeySequence(Qt::ALT | Qt::Key_F) },
		{ QStringLiteral("capture.current_screen"), QStringLiteral("Current screen"), QKeySequence(Qt::ALT | Qt::Key_C) },
		{ QStringLiteral("capture.active_window"), QStringLiteral("Active window"), QKeySequence(Qt::ALT | Qt::Key_A) },
		{ QStringLiteral("capture.select_window"), QStringLiteral("Select window"), QKeySequence(Qt::ALT | Qt::Key_W) },
		{ QStringLiteral("ocr.rect_area"), QStringLiteral("Recognize text in area"), QKeySequence() }
	};
}

PortalShortcutList WaylandGlobalShortcutManagerTests::shortcutMetadata()
{
	PortalShortcutList result;
	for(const auto &shortcut : shortcuts()) {
		result.append(qMakePair(shortcut.id, QVariantMap{
			{ QStringLiteral("description"), shortcut.description },
			{ QStringLiteral("trigger_description"), QStringLiteral("Saved trigger") }
		}));
	}
	return result;
}

bool WaylandGlobalShortcutManagerTests::waitForCreate(int count)
{
	return QTest::qWaitFor([this, count]() { return mPortal->mCreateRequests.count() == count; }, WaitTimeoutMs);
}

bool WaylandGlobalShortcutManagerTests::waitForBind(int count)
{
	return QTest::qWaitFor([this, count]() { return mPortal->mBindRequests.count() == count; }, WaitTimeoutMs);
}

bool WaylandGlobalShortcutManagerTests::watchActivations()
{
	mObservedActivations = 0;
	// Connect after the manager, observing the same ordered wire deliveries.
	return QDBusConnection::sessionBus().connect(PortalService, PortalPath, PortalInterface, QLatin1String("Activated"),
		this, SLOT(observeActivation(QDBusObjectPath,QString,qulonglong,QVariantMap)));
}

void WaylandGlobalShortcutManagerTests::observeActivation(const QDBusObjectPath &session, const QString &id,
															qulonglong timestamp, const QVariantMap &options)
{
	Q_UNUSED(session)
	Q_UNUSED(id)
	Q_UNUSED(timestamp)
	Q_UNUSED(options)
	mObservedActivations++;
}

bool WaylandGlobalShortcutManagerTests::drainClientCalls()
{
	// An ordered frontend round trip also drains methods dispatched by the manager.
	auto message = QDBusMessage::createMethodCall(PortalService, PortalPath,
		QLatin1String("org.freedesktop.DBus.Properties"), QLatin1String("Get"));
	message << PortalInterface << QLatin1String("version");
	QDBusPendingCallWatcher watcher(QDBusConnection::sessionBus().asyncCall(message, WaitTimeoutMs));
	if(!QTest::qWaitFor([&watcher]() { return watcher.isFinished(); }, WaitTimeoutMs)) {
		return false;
	}
	QDBusPendingReply<QDBusVariant> reply = watcher;
	return !reply.isError();
}

bool WaylandGlobalShortcutManagerTests::sendAndObserve(const QString &sessionPath, const QString &id)
{
	auto expected = mObservedActivations + 1;
	mPortal->rawActivated(sessionPath, id);
	return QTest::qWaitFor([this, expected]() { return mObservedActivations == expected; }, WaitTimeoutMs) && drainClientCalls();
}

bool WaylandGlobalShortcutManagerTests::keypressAndObserve(const QString &sessionPath, const QString &id)
{
	auto expected = mObservedActivations + 1;
	if(!mPortal->keypress(sessionPath, id)) {
		return false;
	}
	return QTest::qWaitFor([this, expected]() { return mObservedActivations == expected; }, WaitTimeoutMs) && drainClientCalls();
}
#endif

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

#ifdef Q_OS_LINUX
void WaylandGlobalShortcutManagerTests::Start_Should_BindAllShortcuts_When_SavedIdsMatch()
{
	WaylandGlobalShortcutManager manager;
	QSignalSpy activated(&manager, &WaylandGlobalShortcutManager::activated);
	QVERIFY(watchActivations());
	manager.start(shortcuts());
	QVERIFY2(waitForCreate(1), "Missing CreateSession on the private test bus");
	mPortal->respondCreate(0);
	QVERIFY2(waitForBind(1), "Missing BindShortcuts after CreateSession with six saved IDs; saved metadata must not activate a fresh session");
	QCOMPARE(mPortal->mListRequests.count(), 0);

	const auto bound = mPortal->mBindRequests.first()->mShortcuts;
	const auto requested = shortcuts();
	QCOMPARE(bound.count(), 6);
	for(int i = 0; i < requested.count(); i++) {
		QCOMPARE(bound.at(i).first, requested.at(i).id);
		QCOMPARE(bound.at(i).second.value(QStringLiteral("description")).toString(), requested.at(i).description);
		auto preferred = WaylandGlobalShortcutManager::preferredTrigger(requested.at(i).keySequence);
		if(preferred.isEmpty()) {
			QVERIFY(!bound.at(i).second.contains(QStringLiteral("preferred_trigger")));
		} else {
			QCOMPARE(bound.at(i).second.value(QStringLiteral("preferred_trigger")).toString(), preferred);
		}
	}

	auto session = mPortal->mCreateRequests.first()->mSessionPath;
	QVERIFY(!mPortal->keypress(session, QStringLiteral("ocr.rect_area")));
	mPortal->respondBind(0, shortcutMetadata());
	QVERIFY(keypressAndObserve(session, QStringLiteral("ocr.rect_area")));
	QCOMPARE(activated.count(), 1);
	QCOMPARE(activated.first().first().toString(), QStringLiteral("ocr.rect_area"));
	QCOMPARE(mPortal->mBindRequests.count(), 1);
}

void WaylandGlobalShortcutManagerTests::Activated_Should_EmitOcrOnce_AndFilterInactiveOrUnrequestedIds()
{
	WaylandGlobalShortcutManager manager;
	QSignalSpy activated(&manager, &WaylandGlobalShortcutManager::activated);
	QVERIFY(watchActivations());
	manager.start(shortcuts());
	QVERIFY(waitForCreate(1));
	mPortal->respondCreate(0);
	QVERIFY2(waitForBind(1), "Missing BindShortcuts for a fresh session");
	auto session = mPortal->mCreateRequests.first()->mSessionPath;
	QVERIFY(sendAndObserve(session, QStringLiteral("ocr.rect_area")));
	QCOMPARE(activated.count(), 0);

	// Include an unrequested returned ID to exercise both action filters.
	PortalShortcutList returned{
		shortcutMetadata().last(), { QStringLiteral("unrequested.action"), QVariantMap() }
	};
	mPortal->respondBind(0, returned);
	QVERIFY(sendAndObserve(QStringLiteral("/wrong/session"), QStringLiteral("ocr.rect_area")));
	QVERIFY(sendAndObserve(session, QStringLiteral("capture.rect_area")));
	QVERIFY(sendAndObserve(session, QStringLiteral("unrequested.action")));
	QCOMPARE(activated.count(), 0);
	QVERIFY(!mPortal->keypress(session, QStringLiteral("capture.rect_area")));
	QVERIFY(keypressAndObserve(session, QStringLiteral("ocr.rect_area")));
	QCOMPARE(activated.count(), 1);
	QCOMPARE(activated.first().first().toString(), QStringLiteral("ocr.rect_area"));
}

void WaylandGlobalShortcutManagerTests::Start_Should_RebindOnce_When_SessionIsRecreated_data()
{
	QTest::addColumn<bool>("settingsChanged");
	QTest::newRow("new-start") << false;
	QTest::newRow("changed-settings") << true;
}

void WaylandGlobalShortcutManagerTests::Start_Should_RebindOnce_When_SessionIsRecreated()
{
	QFETCH(bool, settingsChanged);
	WaylandGlobalShortcutManager manager;
	QSignalSpy activated(&manager, &WaylandGlobalShortcutManager::activated);
	QVERIFY(watchActivations());
	manager.start(shortcuts());
	QVERIFY(waitForCreate(1));
	mPortal->respondCreate(0);
	QVERIFY2(waitForBind(1), "Missing first BindShortcuts");
	auto oldSession = mPortal->mCreateRequests.first()->mSessionPath;
	mPortal->respondBind(0, shortcutMetadata());
	QVERIFY(sendAndObserve(oldSession, QStringLiteral("test.delivery")));

	auto changed = shortcuts();
	if(settingsChanged) {
		changed.last().keySequence = QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_O);
	}
	manager.start(changed);
	QVERIFY(waitForCreate(2));
	QCOMPARE(mPortal->mSessions.value(oldSession)->mCloseCount, 1);
	QCOMPARE(mPortal->mSavedShortcuts.count(), 6);
	mPortal->respondCreate(1);
	QVERIFY2(waitForBind(2), "Missing fresh BindShortcuts when recreating a session with saved IDs");
	auto newSession = mPortal->mCreateRequests.last()->mSessionPath;
	QVERIFY(newSession != oldSession);
	QCOMPARE(mPortal->mBindRequests.first()->mSessionPath, oldSession);
	QCOMPARE(mPortal->mBindRequests.last()->mSessionPath, newSession);
	QCOMPARE(mPortal->mBindRequests.last()->mShortcuts.count(), 6);
	if(settingsChanged) {
		QCOMPARE(mPortal->mBindRequests.last()->mShortcuts.last().second.value(QStringLiteral("preferred_trigger")).toString(),
			QStringLiteral("ALT+SHIFT+o"));
	}
	mPortal->respondBind(1, shortcutMetadata());
	QVERIFY(sendAndObserve(oldSession, QStringLiteral("ocr.rect_area")));
	QVERIFY(keypressAndObserve(newSession, QStringLiteral("ocr.rect_area")));
	QCOMPARE(activated.count(), 1);
	QCOMPARE(mPortal->mBindRequests.count(), 2);
	QCOMPARE(mPortal->mListRequests.count(), 0);
}

void WaylandGlobalShortcutManagerTests::BindShortcuts_Should_AcceptFullSubsetAndTypedEmptyResults_data()
{
	QTest::addColumn<PortalShortcutList>("returned");
	QTest::addColumn<int>("activationCount");
	QTest::newRow("full") << shortcutMetadata() << 1;
	QTest::newRow("subset") << PortalShortcutList{ shortcutMetadata().last() } << 1;
	QTest::newRow("typed-empty") << PortalShortcutList() << 0;
}

void WaylandGlobalShortcutManagerTests::BindShortcuts_Should_AcceptFullSubsetAndTypedEmptyResults()
{
	QFETCH(PortalShortcutList, returned);
	QFETCH(int, activationCount);
	WaylandGlobalShortcutManager manager;
	QSignalSpy activated(&manager, &WaylandGlobalShortcutManager::activated);
	QVERIFY(watchActivations());
	manager.start(shortcuts());
	QVERIFY(waitForCreate(1));
	mPortal->respondCreate(0);
	QVERIFY2(waitForBind(1), "Missing BindShortcuts before testing its response");
	auto session = mPortal->mCreateRequests.first()->mSessionPath;
	mPortal->respondBind(0, returned);
	QVERIFY(sendAndObserve(session, QStringLiteral("ocr.rect_area")));
	QCOMPARE(activated.count(), activationCount);
	QCOMPARE(mPortal->mSessions.value(session)->mCloseCount, 0);
	QCOMPARE(mPortal->mBindRequests.count(), 1);
	QCOMPARE(mPortal->mListRequests.count(), 0);
	// Even a valid empty binding completes the session and permits v2 configuration.
	manager.requestConfigureShortcuts();
	QTRY_COMPARE_WITH_TIMEOUT(mPortal->mConfiguredSessions.count(), 1, WaitTimeoutMs);
	QCOMPARE(mPortal->mConfiguredSessions.first(), session);
}

void WaylandGlobalShortcutManagerTests::ConfigureShortcuts_Should_WaitForBind_AndRespectPortalVersion_data()
{
	QTest::addColumn<uint>("version");
	QTest::newRow("v2") << uint(2);
	QTest::newRow("v1") << uint(1);
}

void WaylandGlobalShortcutManagerTests::ConfigureShortcuts_Should_WaitForBind_AndRespectPortalVersion()
{
	QFETCH(uint, version);
	mPortal->mVersion = version;
	WaylandGlobalShortcutManager manager;
	QVERIFY(watchActivations());
	manager.start(shortcuts());
	manager.requestConfigureShortcuts();
	QVERIFY(waitForCreate(1));
	mPortal->respondCreate(0);
	QVERIFY2(waitForBind(1), "Missing BindShortcuts while configuration is queued");
	QCOMPARE(mPortal->mConfiguredSessions.count(), 0);
	auto session = mPortal->mCreateRequests.first()->mSessionPath;
	mPortal->respondBind(0, shortcutMetadata());
	if(version == 2) {
		QTRY_COMPARE_WITH_TIMEOUT(mPortal->mConfiguredSessions.count(), 1, WaitTimeoutMs);
		QCOMPARE(mPortal->mConfiguredSessions.first(), session);
	} else {
		QVERIFY(sendAndObserve(session, QStringLiteral("test.delivery")));
		QCOMPARE(mPortal->mConfiguredSessions.count(), 0);
	}
	QCOMPARE(mPortal->mBindRequests.count(), 1);
	QCOMPARE(mPortal->mListRequests.count(), 0);
}

void WaylandGlobalShortcutManagerTests::BindShortcuts_Should_StopWithoutRetry_When_CanceledDeniedOrMethodFails_data()
{
	QTest::addColumn<uint>("response");
	QTest::addColumn<bool>("methodError");
	QTest::newRow("cancel") << uint(1) << false;
	QTest::newRow("deny") << uint(2) << false;
	QTest::newRow("method-error") << uint(0) << true;
}

void WaylandGlobalShortcutManagerTests::BindShortcuts_Should_StopWithoutRetry_When_CanceledDeniedOrMethodFails()
{
	QFETCH(uint, response);
	QFETCH(bool, methodError);
	mPortal->mBindMethodError = methodError;
	WaylandGlobalShortcutManager manager;
	QSignalSpy activated(&manager, &WaylandGlobalShortcutManager::activated);
	QVERIFY(watchActivations());
	manager.start(shortcuts());
	manager.requestConfigureShortcuts();
	QVERIFY(waitForCreate(1));
	mPortal->respondCreate(0);
	QVERIFY2(waitForBind(1), "Missing BindShortcuts before testing cancellation or failure");
	auto session = mPortal->mCreateRequests.first()->mSessionPath;
	if(!methodError) {
		mPortal->respondBind(0, shortcutMetadata(), response);
	}
	QTRY_COMPARE_WITH_TIMEOUT(mPortal->mSessions.value(session)->mCloseCount, 1, WaitTimeoutMs);
	QVERIFY(sendAndObserve(session, QStringLiteral("ocr.rect_area")));
	QCOMPARE(activated.count(), 0);
	QCOMPARE(mPortal->mConfiguredSessions.count(), 0);
	QCOMPARE(mPortal->mCreateRequests.count(), 1);
	QCOMPARE(mPortal->mBindRequests.count(), 1);
	QCOMPARE(mPortal->mListRequests.count(), 0);
}

void WaylandGlobalShortcutManagerTests::Stop_Should_RejectPendingBindCompletion_When_Restarted()
{
	WaylandGlobalShortcutManager manager;
	QSignalSpy activated(&manager, &WaylandGlobalShortcutManager::activated);
	QVERIFY(watchActivations());
	manager.start(shortcuts());
	QVERIFY(waitForCreate(1));
	mPortal->respondCreate(0);
	QVERIFY2(waitForBind(1), "Missing pending BindShortcuts");
	auto oldSession = mPortal->mCreateRequests.first()->mSessionPath;
	manager.stop();
	manager.start(shortcuts());
	QVERIFY(waitForCreate(2));
	QCOMPARE(mPortal->mBindRequests.first()->mCloseCount, 1);
	QCOMPARE(mPortal->mSessions.value(oldSession)->mCloseCount, 1);
	mPortal->respondCreate(1);
	QVERIFY2(waitForBind(2), "Missing BindShortcuts in restarted session");
	auto newSession = mPortal->mCreateRequests.last()->mSessionPath;
	mPortal->respondBind(0, shortcutMetadata());
	QVERIFY(sendAndObserve(oldSession, QStringLiteral("ocr.rect_area")));
	QVERIFY(sendAndObserve(newSession, QStringLiteral("ocr.rect_area")));
	QCOMPARE(activated.count(), 0);
	mPortal->respondBind(1, shortcutMetadata());
	QVERIFY(keypressAndObserve(newSession, QStringLiteral("ocr.rect_area")));
	QCOMPARE(activated.count(), 1);
	QCOMPARE(mPortal->mCreateRequests.count(), 2);
	QCOMPARE(mPortal->mBindRequests.count(), 2);
	QCOMPARE(mPortal->mListRequests.count(), 0);
}
#endif

TEST_MAIN(WaylandGlobalShortcutManagerTests)
