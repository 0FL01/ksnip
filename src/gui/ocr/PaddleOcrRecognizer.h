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

#ifndef KSNIP_PADDLEOCRRECOGNIZER_H
#define KSNIP_PADDLEOCRRECOGNIZER_H

#include <onnxruntime_cxx_api.h>

#include <clipper.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <QImage>
#include <QDebug>
#include <QResource>

#include "src/gui/ocr/IOcrRecognizer.h"

class PaddleOcrRecognizer : public IOcrRecognizer
{
public:
	PaddleOcrRecognizer();
	~PaddleOcrRecognizer() override;

	OcrResult recognize(const QImage &image) override;

private:
	class Implementation;

	std::mutex mMutex;
	std::unique_ptr<Implementation> mImplementation;
};

#endif // KSNIP_PADDLEOCRRECOGNIZER_H
