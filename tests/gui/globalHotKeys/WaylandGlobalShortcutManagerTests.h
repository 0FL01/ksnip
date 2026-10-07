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
#include <memory>
#include <QUuid>

#include "src/gui/globalHotKeys/WaylandGlobalShortcutManager.h"
#include "tests/utils/TestRunner.h"

#ifdef Q_OS_LINUX
class ShortcutPortalRequestFake : public QObject
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Request")
public:
	QString mPath;
	QString mSessionPath;
	PortalShortcutList mShortcuts;
	int mCloseCount = 0;

	explicit ShortcutPortalRequestFake(QObject *parent);

public slots:
	void Close();

signals:
	void Response(uint response, const QVariantMap &results);
};

class ShortcutPortalSessionFake : public QObject
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Session")
public:
	QString mPath;
	QSet<QString> mBoundIds;
	bool mBound = false;
	int mCloseCount = 0;

	explicit ShortcutPortalSessionFake(QObject *parent);

public slots:
	void Close();
};

class ShortcutPortalFake : public QObject, protected QDBusContext
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.GlobalShortcuts")
	Q_PROPERTY(uint version READ version)
public:
	QList<ShortcutPortalRequestFake *> mCreateRequests;
	QList<ShortcutPortalRequestFake *> mBindRequests;
	QList<ShortcutPortalRequestFake *> mListRequests;
	QHash<QString, ShortcutPortalSessionFake *> mSessions;
	QStringList mConfiguredSessions;
	PortalShortcutList mSavedShortcuts;
	QString mError;
	uint mVersion = 2;
	bool mBindMethodError = false;

	~ShortcutPortalFake() override;
	bool start();
	uint version() const;
	void respondCreate(int index);
	void respondBind(int index, const PortalShortcutList &shortcuts, uint response = 0);
	bool keypress(const QString &sessionPath, const QString &id);
	void rawActivated(const QString &sessionPath, const QString &id);

public slots:
	QDBusObjectPath CreateSession(const QVariantMap &options);
	QDBusObjectPath BindShortcuts(const QDBusObjectPath &session, const PortalShortcutList &shortcuts,
									 const QString &parentWindow, const QVariantMap &options);
	QDBusObjectPath ListShortcuts(const QDBusObjectPath &session, const QVariantMap &options);
	void ConfigureShortcuts(const QDBusObjectPath &session, const QString &parentWindow, const QVariantMap &options);

signals:
	void Activated(const QDBusObjectPath &session, const QString &id, qulonglong timestamp, const QVariantMap &options);

private:
	std::unique_ptr<QDBusConnection> mConnection;
	QString mConnectionName;
	bool mServiceRegistered = false;

	QString handlePath(const QString &kind, const QString &token) const;
	ShortcutPortalRequestFake *createRequest(const QVariantMap &options, const QString &sessionPath);
	bool registerObject(const QString &path, QObject *object);
};
#endif

class WaylandGlobalShortcutManagerTests : public QObject
{
	Q_OBJECT
private slots:
	void PreferredTrigger_Should_ConvertSupportedSingleChord();
	void PreferredTrigger_Should_ReturnEmpty_When_SequenceIsUnsafe();
	void RegisterDBusTypes_Should_RegisterShortcutListSignature();
#ifdef Q_OS_LINUX
	void init();
	void cleanup();
	void Start_Should_BindAllShortcuts_When_SavedIdsMatch();
	void Activated_Should_EmitOcrOnce_AndFilterInactiveOrUnrequestedIds();
	void Start_Should_RebindOnce_When_SessionIsRecreated_data();
	void Start_Should_RebindOnce_When_SessionIsRecreated();
	void BindShortcuts_Should_AcceptFullSubsetAndTypedEmptyResults_data();
	void BindShortcuts_Should_AcceptFullSubsetAndTypedEmptyResults();
	void ConfigureShortcuts_Should_WaitForBind_AndRespectPortalVersion_data();
	void ConfigureShortcuts_Should_WaitForBind_AndRespectPortalVersion();
	void BindShortcuts_Should_StopWithoutRetry_When_CanceledDeniedOrMethodFails_data();
	void BindShortcuts_Should_StopWithoutRetry_When_CanceledDeniedOrMethodFails();
	void Stop_Should_RejectPendingBindCompletion_When_Restarted();
	void observeActivation(const QDBusObjectPath &session, const QString &id, qulonglong timestamp, const QVariantMap &options);

private:
	std::unique_ptr<ShortcutPortalFake> mPortal;
	int mObservedActivations = 0;

	static QList<WaylandGlobalShortcutManager::Shortcut> shortcuts();
	static PortalShortcutList shortcutMetadata();
	bool waitForCreate(int count);
	bool waitForBind(int count);
	bool watchActivations();
	bool drainClientCalls();
	bool sendAndObserve(const QString &sessionPath, const QString &id);
	bool keypressAndObserve(const QString &sessionPath, const QString &id);
#endif
};

#endif // KSNIP_WAYLANDGLOBALSHORTCUTMANAGERTESTS_H
