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

#ifndef KSNIP_WAYLANDGLOBALSHORTCUTMANAGER_H
#define KSNIP_WAYLANDGLOBALSHORTCUTMANAGER_H

#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusVariant>
#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

using PortalShortcut = QPair<QString, QVariantMap>;
using PortalShortcutList = QList<PortalShortcut>;

Q_DECLARE_METATYPE(PortalShortcut)
Q_DECLARE_METATYPE(PortalShortcutList)

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcut &shortcut);
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcut &shortcut);

class WaylandGlobalShortcutManager : public QObject, protected QDBusContext
{
	Q_OBJECT
public:
	struct Shortcut
	{
		QString id;
		QString description;
		QKeySequence keySequence;
	};

	explicit WaylandGlobalShortcutManager(QObject *parent = nullptr);
	~WaylandGlobalShortcutManager() override;

	void start(const QList<Shortcut> &shortcuts);
	void stop();
	void requestConfigureShortcuts();

	static QString preferredTrigger(const QKeySequence &keySequence);
	static void registerDBusTypes();

signals:
	void activated(const QString &shortcutId);

private:
	enum class RequestKind
	{
		Create,
		List,
		Bind
	};

	struct Request
	{
		RequestKind kind;
		quint64 generation;
		quint64 id;
	};

	QList<Shortcut> mShortcuts;
	QSet<QString> mDesiredIds;
	QSet<QString> mBoundIds;
	QHash<QString, Request> mRequests;
	QString mCurrentRequestPath;
	QString mSessionPath;
	quint64 mGeneration;
	quint64 mTokenCounter;
	quint64 mRequestCounter;
	uint mPortalVersion;
	bool mReconciled;
	bool mConfigureRequested;

	QString createToken(const QString &prefix);
	QString predictedRequestPath(const QString &token) const;
	void probe(quint64 generation);
	void createSession(quint64 generation);
	void listShortcuts(quint64 generation);
	void bindShortcuts(quint64 generation);
	void configureShortcutsIfReady();
	void beginRequest(RequestKind kind, quint64 generation, const QString &method, const QVariantList &arguments, const QString &token);
	void processRequestReply(const QString &expectedPath, quint64 requestId, QDBusPendingCallWatcher *watcher);
	void processCreateResponse(const QVariantMap &results, quint64 generation);
	void processListResponse(const QVariantMap &results, quint64 generation);
	void processBindResponse(const QVariantMap &results, quint64 generation);
	void setActive(const QSet<QString> &boundIds);
	void fail(const QString &error);
	void closeRequest(const QString &path, quint64 requestId);
	static void closeSession(const QString &path);
	static QSet<QString> shortcutIds(const QVariantMap &results, bool *ok);
	static PortalShortcutList portalShortcuts(const QList<Shortcut> &shortcuts);
	static QString keyName(int key);

private slots:
	void portalResponse(uint response, const QVariantMap &results);
	void portalActivated(const QDBusObjectPath &sessionHandle, const QString &shortcutId, qulonglong timestamp, const QVariantMap &options);
};

#endif // KSNIP_WAYLANDGLOBALSHORTCUTMANAGER_H
