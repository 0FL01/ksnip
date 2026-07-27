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

#include "SaveOperationTests.h"

#include "src/gui/operations/SaveOperation.h"

#include "tests/utils/TestRunner.h"
#include "tests/mocks/backend/config/ConfigMock.h"
#include "tests/mocks/backend/recentImages/RecentImageServiceMock.h"
#include "tests/mocks/backend/saver/ImageSaverMock.h"
#include "tests/mocks/backend/saver/SavePathProviderMock.h"
#include "tests/mocks/gui/NotificationServiceMock.h"

void SaveOperationTests::Execute_Should_ShowSuccessToast_When_SuccessToastEnabled()
{
	// arrange
	QWidget parent;
	auto notificationServiceMock = QSharedPointer<NotificationServiceMock>(new NotificationServiceMock);
	auto recentImageServiceMock = QSharedPointer<RecentImageServiceMock>(new RecentImageServiceMock);
	auto imageSaverMock = QSharedPointer<ImageSaverMock>(new ImageSaverMock);
	auto savePathProviderMock = QSharedPointer<SavePathProviderMock>(new SavePathProviderMock);
	auto configMock = QSharedPointer<ConfigMock>(new ConfigMock);
	auto path = QStringLiteral("capture.png");

	EXPECT_CALL(*savePathProviderMock, savePath()).WillOnce(testing::Return(path));
	EXPECT_CALL(*imageSaverMock, save(testing::_, path)).WillOnce(testing::Return(true));
	EXPECT_CALL(*recentImageServiceMock, storeImagePath(path));
	EXPECT_CALL(*configMock, trayIconNotificationsEnabled()).WillOnce(testing::Return(true));
	EXPECT_CALL(*notificationServiceMock, showInfo(testing::_, testing::_, path));
	EXPECT_CALL(*notificationServiceMock, showWarning(testing::_, testing::_, testing::_)).Times(0);
	EXPECT_CALL(*notificationServiceMock, showCritical(testing::_, testing::_, testing::_)).Times(0);

	SaveOperation operation(
			QImage(1, 1, QImage::Format_ARGB32),
			true,
			notificationServiceMock,
			recentImageServiceMock,
			imageSaverMock,
			savePathProviderMock,
			nullptr,
			configMock,
			&parent);

	// act
	auto result = operation.execute(true);

	// assert
	QVERIFY(result.isSuccessful);
}

void SaveOperationTests::Execute_Should_NotShowToast_When_SuccessToastDisabled()
{
	// arrange
	QWidget parent;
	auto notificationServiceMock = QSharedPointer<NotificationServiceMock>(new NotificationServiceMock);
	auto recentImageServiceMock = QSharedPointer<RecentImageServiceMock>(new RecentImageServiceMock);
	auto imageSaverMock = QSharedPointer<ImageSaverMock>(new ImageSaverMock);
	auto savePathProviderMock = QSharedPointer<SavePathProviderMock>(new SavePathProviderMock);
	auto configMock = QSharedPointer<ConfigMock>(new ConfigMock);
	auto path = QStringLiteral("capture.png");

	EXPECT_CALL(*savePathProviderMock, savePath()).WillOnce(testing::Return(path));
	EXPECT_CALL(*imageSaverMock, save(testing::_, path)).WillOnce(testing::Return(true));
	EXPECT_CALL(*recentImageServiceMock, storeImagePath(path));
	EXPECT_CALL(*notificationServiceMock, showInfo(testing::_, testing::_, testing::_)).Times(0);
	EXPECT_CALL(*notificationServiceMock, showWarning(testing::_, testing::_, testing::_)).Times(0);
	EXPECT_CALL(*notificationServiceMock, showCritical(testing::_, testing::_, testing::_)).Times(0);

	SaveOperation operation(
			QImage(1, 1, QImage::Format_ARGB32),
			true,
			notificationServiceMock,
			recentImageServiceMock,
			imageSaverMock,
			savePathProviderMock,
			nullptr,
			configMock,
			&parent);

	// act
	auto result = operation.execute(false);

	// assert
	QVERIFY(result.isSuccessful);
}

void SaveOperationTests::Execute_Should_ShowCriticalToast_When_SaveFailsAndSuccessToastDisabled()
{
	// arrange
	QWidget parent;
	auto notificationServiceMock = QSharedPointer<NotificationServiceMock>(new NotificationServiceMock);
	auto recentImageServiceMock = QSharedPointer<RecentImageServiceMock>(new RecentImageServiceMock);
	auto imageSaverMock = QSharedPointer<ImageSaverMock>(new ImageSaverMock);
	auto savePathProviderMock = QSharedPointer<SavePathProviderMock>(new SavePathProviderMock);
	auto configMock = QSharedPointer<ConfigMock>(new ConfigMock);
	auto path = QStringLiteral("capture.png");

	EXPECT_CALL(*savePathProviderMock, savePath()).WillOnce(testing::Return(path));
	EXPECT_CALL(*imageSaverMock, save(testing::_, path)).WillOnce(testing::Return(false));
	EXPECT_CALL(*recentImageServiceMock, storeImagePath(testing::_)).Times(0);
	EXPECT_CALL(*configMock, trayIconNotificationsEnabled()).WillOnce(testing::Return(true));
	EXPECT_CALL(*notificationServiceMock, showInfo(testing::_, testing::_, testing::_)).Times(0);
	EXPECT_CALL(*notificationServiceMock, showWarning(testing::_, testing::_, testing::_)).Times(0);
	EXPECT_CALL(*notificationServiceMock, showCritical(testing::_, testing::_, path));

	SaveOperation operation(
			QImage(1, 1, QImage::Format_ARGB32),
			true,
			notificationServiceMock,
			recentImageServiceMock,
			imageSaverMock,
			savePathProviderMock,
			nullptr,
			configMock,
			&parent);

	// act
	auto result = operation.execute(false);

	// assert
	QVERIFY(!result.isSuccessful);
}

TEST_MAIN(SaveOperationTests)
