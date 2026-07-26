/*
 * Copyright (C) 2017 Damir Porobic <https://github.com/damirporobic>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef KSNIP_KWINSCREENSHOT2CLIENTTESTS_H
#define KSNIP_KWINSCREENSHOT2CLIENTTESTS_H

#include <QtTest>

class KWinScreenShot2ClientTests : public QObject
{
	Q_OBJECT
private slots:
	void ReadImage_Should_ReturnCompleteImage_When_MetadataAndPayloadAreValid();
	void ReadImage_Should_Fail_When_PayloadEndsEarly();
	void ReadImage_Should_Fail_When_ReadTimesOut();
	void ReadImage_Should_CloseDescriptor_When_MetadataIsInvalid();
	void CaptureArea_Should_FailWithoutDBusCall_When_SizeIsEmpty();
};

#endif // KSNIP_KWINSCREENSHOT2CLIENTTESTS_H
