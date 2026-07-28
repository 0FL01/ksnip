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
 *
 * DB geometry, crop, preprocessing and decoding behavior are adapted
 * from PaddleOCR commit 2661c7c0ef5c613e8f93c6e93b2e052399f0f854
 * (Apache-2.0). Clipper 6.4.2 is distributed under BSL-1.0.
 */

#include "PaddleOcrRecognizer.h"

namespace {

using Box = std::array<cv::Point2f, 4>;

struct Tensor
{
	std::vector<float> values;
	std::vector<int64_t> shape;
};

class InferenceModel
{
public:
	InferenceModel(Ort::Env &environment, const uchar *data, size_t size) :
		mSession(nullptr)
	{
		Ort::SessionOptions options;
		options.SetIntraOpNumThreads(1);
		options.SetInterOpNumThreads(1);
		options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
		mSession = Ort::Session(environment, data, size, options);

		Ort::AllocatorWithDefaultOptions allocator;
		auto inputName = mSession.GetInputNameAllocated(0, allocator);
		auto outputName = mSession.GetOutputNameAllocated(0, allocator);
		mInputName = inputName.get();
		mOutputName = outputName.get();
	}

	Tensor run(const Tensor &input)
	{
		auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
		auto inputValue = Ort::Value::CreateTensor<float>(
			memoryInfo,
			const_cast<float *>(input.values.data()),
			input.values.size(),
			input.shape.data(),
			input.shape.size());
		const char *inputNames[] = {mInputName.c_str()};
		const char *outputNames[] = {mOutputName.c_str()};
		Ort::RunOptions runOptions;
		auto outputs = mSession.Run(runOptions, inputNames, &inputValue, 1, outputNames, 1);
		auto typeInfo = outputs[0].GetTensorTypeAndShapeInfo();
		Tensor result;
		result.shape = typeInfo.GetShape();
		auto count = typeInfo.GetElementCount();
		const auto *values = outputs[0].GetTensorData<float>();
		result.values.assign(values, values + count);
		return result;
	}

private:
	Ort::Session mSession;
	std::string mInputName;
	std::string mOutputName;
};

const uchar *resourceData(const QResource &resource)
{
	if (!resource.isValid() || resource.size() <= 0 || resource.data() == nullptr) {
		throw std::runtime_error("Missing embedded OCR resource");
	}
	return resource.data();
}

cv::Mat toBgrImage(const QImage &image)
{
	if (image.isNull()) {
		throw std::runtime_error("Cannot recognize an empty image");
	}

	auto rgba = image.convertToFormat(QImage::Format_RGBA8888);
	cv::Mat result(rgba.height(), rgba.width(), CV_8UC3);
	for (int y = 0; y < rgba.height(); ++y) {
		const auto *source = rgba.constScanLine(y);
		auto *destination = result.ptr<cv::Vec3b>(y);
		for (int x = 0; x < rgba.width(); ++x) {
			const auto *pixel = source + x * 4;
			const auto alpha = static_cast<int>(pixel[3]);
			const auto flatten = [alpha](int channel) {
				return static_cast<uchar>((channel * alpha + 255 * (255 - alpha) + 127) / 255);
			};
			destination[x] = cv::Vec3b(flatten(pixel[2]), flatten(pixel[1]), flatten(pixel[0]));
		}
	}
	return result;
}

Tensor preprocessDetector(const cv::Mat &image)
{
	const auto ratio = 960.0f / static_cast<float>(std::max(image.rows, image.cols));
	auto resizedHeight = static_cast<int>(image.rows * ratio);
	auto resizedWidth = static_cast<int>(image.cols * ratio);
	resizedHeight = (resizedHeight + 127) / 128 * 128;
	resizedWidth = (resizedWidth + 127) / 128 * 128;
	cv::Mat resized;
	cv::resize(image, resized, cv::Size(resizedWidth, resizedHeight));

	constexpr std::array<float, 3> mean = {0.485f, 0.456f, 0.406f};
	constexpr std::array<float, 3> standardDeviation = {0.229f, 0.224f, 0.225f};
	Tensor result;
	result.shape = {1, 3, resizedHeight, resizedWidth};
	result.values.resize(static_cast<size_t>(3 * resizedHeight * resizedWidth));
	const auto planeSize = static_cast<size_t>(resizedHeight * resizedWidth);
	for (int y = 0; y < resizedHeight; ++y) {
		for (int x = 0; x < resizedWidth; ++x) {
			const auto pixel = resized.at<cv::Vec3b>(y, x);
			for (int channel = 0; channel < 3; ++channel) {
				result.values[channel * planeSize + static_cast<size_t>(y * resizedWidth + x)] =
					(pixel[channel] / 255.0f - mean[channel]) / standardDeviation[channel];
			}
		}
	}
	return result;
}

std::pair<std::vector<cv::Point2f>, float> getMiniBox(const std::vector<cv::Point2f> &contour)
{
	auto rectangle = cv::minAreaRect(contour);
	std::vector<cv::Point2f> points(4);
	rectangle.points(points.data());
	std::sort(points.begin(), points.end(), [](const auto &left, const auto &right) {
		return left.x < right.x;
	});
	const auto index1 = points[1].y > points[0].y ? 0 : 1;
	const auto index4 = points[1].y > points[0].y ? 1 : 0;
	const auto index2 = points[3].y > points[2].y ? 2 : 3;
	const auto index3 = points[3].y > points[2].y ? 3 : 2;
	return {{points[index1], points[index2], points[index3], points[index4]},
		std::min(rectangle.size.width, rectangle.size.height)};
}

float boxScore(const cv::Mat &probability, std::vector<cv::Point2f> box)
{
	auto minX = box.front().x;
	auto maxX = box.front().x;
	auto minY = box.front().y;
	auto maxY = box.front().y;
	for (const auto &point : box) {
		minX = std::min(minX, point.x);
		maxX = std::max(maxX, point.x);
		minY = std::min(minY, point.y);
		maxY = std::max(maxY, point.y);
	}
	const auto x0 = std::clamp(static_cast<int>(std::floor(minX)), 0, probability.cols - 1);
	const auto x1 = std::clamp(static_cast<int>(std::ceil(maxX)), 0, probability.cols - 1);
	const auto y0 = std::clamp(static_cast<int>(std::floor(minY)), 0, probability.rows - 1);
	const auto y1 = std::clamp(static_cast<int>(std::ceil(maxY)), 0, probability.rows - 1);
	cv::Mat mask = cv::Mat::zeros(y1 - y0 + 1, x1 - x0 + 1, CV_8UC1);
	std::vector<cv::Point> integerBox;
	for (auto &point : box) {
		integerBox.emplace_back(static_cast<int>(point.x - x0), static_cast<int>(point.y - y0));
	}
	cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{integerBox}, cv::Scalar(1));
	return static_cast<float>(cv::mean(probability(cv::Rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1)), mask)[0]);
}

std::vector<cv::Point2f> unclip(const std::vector<cv::Point2f> &box)
{
	const auto length = cv::arcLength(box, true);
	if (length <= 0) {
		return {};
	}
	const auto distance = std::abs(cv::contourArea(box)) * 1.5 / length;
	ClipperLib::Path path;
	for (const auto &point : box) {
		path.emplace_back(static_cast<ClipperLib::cInt>(point.x), static_cast<ClipperLib::cInt>(point.y));
	}
	ClipperLib::ClipperOffset offset;
	offset.AddPath(path, ClipperLib::jtRound, ClipperLib::etClosedPolygon);
	ClipperLib::Paths solutions;
	offset.Execute(solutions, distance);
	if (solutions.size() != 1) {
		return {};
	}
	std::vector<cv::Point2f> result;
	for (const auto &point : solutions.front()) {
		result.emplace_back(static_cast<float>(point.X), static_cast<float>(point.Y));
	}
	return result;
}

Box orderBox(const std::vector<cv::Point2f> &points)
{
	if (points.size() != 4) {
		throw std::runtime_error("Expected four box points");
	}
	auto minSum = 0U;
	auto maxSum = 0U;
	for (size_t i = 1; i < points.size(); ++i) {
		if (points[i].x + points[i].y < points[minSum].x + points[minSum].y) {
			minSum = i;
		}
		if (points[i].x + points[i].y > points[maxSum].x + points[maxSum].y) {
			maxSum = i;
		}
	}
	std::vector<size_t> remaining;
	for (size_t i = 0; i < points.size(); ++i) {
		if (i != minSum && i != maxSum) {
			remaining.push_back(i);
		}
	}
	const auto firstDifference = points[remaining[0]].y - points[remaining[0]].x;
	const auto secondDifference = points[remaining[1]].y - points[remaining[1]].x;
	const auto minDifference = firstDifference < secondDifference ? remaining[0] : remaining[1];
	const auto maxDifference = firstDifference < secondDifference ? remaining[1] : remaining[0];
	return {points[minSum], points[minDifference], points[maxSum], points[maxDifference]};
}

std::vector<Box> detectorBoxes(const Tensor &output, int sourceWidth, int sourceHeight)
{
	if (output.shape.size() != 4 || output.shape[0] != 1 || output.shape[1] != 1) {
		throw std::runtime_error("Unexpected detector output shape");
	}
	const auto height = static_cast<int>(output.shape[2]);
	const auto width = static_cast<int>(output.shape[3]);
	cv::Mat probability(height, width, CV_32FC1, const_cast<float *>(output.values.data()));
	cv::Mat bitmap;
	cv::compare(probability, 0.3, bitmap, cv::CMP_GT);
	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(bitmap, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

	std::vector<Box> boxes;
	for (size_t index = 0; index < std::min<size_t>(contours.size(), 1000); ++index) {
		std::vector<cv::Point2f> contour;
		for (const auto &point : contours[index]) {
			contour.emplace_back(static_cast<float>(point.x), static_cast<float>(point.y));
		}
		auto [miniBox, shortSide] = getMiniBox(contour);
		if (shortSide < 3 || boxScore(probability, miniBox) < 0.6f) {
			continue;
		}
		auto expanded = unclip(miniBox);
		if (expanded.empty()) {
			continue;
		}
		auto [expandedBox, expandedShortSide] = getMiniBox(expanded);
		if (expandedShortSide < 5) {
			continue;
		}
		for (auto &point : expandedBox) {
			point.x = static_cast<float>(std::clamp(
				static_cast<int>(std::nearbyint(point.x / width * sourceWidth)), 0, sourceWidth - 1));
			point.y = static_cast<float>(std::clamp(
				static_cast<int>(std::nearbyint(point.y / height * sourceHeight)), 0, sourceHeight - 1));
		}
		auto ordered = orderBox(expandedBox);
		if (cv::norm(ordered[0] - ordered[1]) <= 3 || cv::norm(ordered[0] - ordered[3]) <= 3) {
			continue;
		}
		boxes.push_back(ordered);
	}

	std::sort(boxes.begin(), boxes.end(), [](const auto &left, const auto &right) {
		return left[0].y == right[0].y ? left[0].x < right[0].x : left[0].y < right[0].y;
	});
	for (size_t i = 0; i + 1 < boxes.size(); ++i) {
		for (size_t j = i + 1; j > 0; --j) {
			if (std::abs(boxes[j][0].y - boxes[j - 1][0].y) < 10 &&
				boxes[j][0].x < boxes[j - 1][0].x) {
				std::swap(boxes[j], boxes[j - 1]);
			} else {
				break;
			}
		}
	}
	return boxes;
}

cv::Mat cropBox(const cv::Mat &image, const Box &box)
{
	const auto width = static_cast<int>(std::max(cv::norm(box[0] - box[1]), cv::norm(box[2] - box[3])));
	const auto height = static_cast<int>(std::max(cv::norm(box[0] - box[3]), cv::norm(box[1] - box[2])));
	if (width <= 0 || height <= 0) {
		return {};
	}
	const std::array<cv::Point2f, 4> destination = {
		cv::Point2f(0, 0), cv::Point2f(width, 0), cv::Point2f(width, height), cv::Point2f(0, height)};
	auto transform = cv::getPerspectiveTransform(box.data(), destination.data());
	cv::Mat crop;
	cv::warpPerspective(image, crop, transform, cv::Size(width, height), cv::INTER_CUBIC, cv::BORDER_REPLICATE);
	if (static_cast<float>(crop.rows) / crop.cols >= 1.5f) {
		cv::rotate(crop, crop, cv::ROTATE_90_COUNTERCLOCKWISE);
	}
	return crop;
}

Tensor preprocessRecognizer(const cv::Mat &crop)
{
	const auto ratio = static_cast<float>(crop.cols) / crop.rows;
	const auto modelWidth = std::min(static_cast<int>(48 * std::max(320.0f / 48.0f, ratio)), 3200);
	const auto resizedWidth = std::min(modelWidth, static_cast<int>(std::ceil(48 * ratio)));
	cv::Mat resized;
	cv::resize(crop, resized, cv::Size(resizedWidth, 48));
	Tensor result;
	result.shape = {1, 3, 48, modelWidth};
	result.values.assign(static_cast<size_t>(3 * 48 * modelWidth), 0.0f);
	const auto planeSize = static_cast<size_t>(48 * modelWidth);
	for (int y = 0; y < 48; ++y) {
		for (int x = 0; x < resizedWidth; ++x) {
			const auto pixel = resized.at<cv::Vec3b>(y, x);
			for (int channel = 0; channel < 3; ++channel) {
				result.values[channel * planeSize + static_cast<size_t>(y * modelWidth + x)] =
					(pixel[channel] / 255.0f - 0.5f) / 0.5f;
			}
		}
	}
	return result;
}

std::string decode(const Tensor &output, const std::vector<std::string> &characters)
{
	if (output.shape.size() != 3 || output.shape[0] != 1) {
		throw std::runtime_error("Unexpected recognizer output shape");
	}
	const auto steps = static_cast<size_t>(output.shape[1]);
	const auto classes = static_cast<size_t>(output.shape[2]);
	if (classes != characters.size() + 2) {
		throw std::runtime_error("Recognizer dictionary does not match output classes");
	}
	std::string text;
	size_t previous = classes;
	for (size_t step = 0; step < steps; ++step) {
		const auto begin = output.values.begin() + static_cast<std::ptrdiff_t>(step * classes);
		const auto best = static_cast<size_t>(std::distance(begin, std::max_element(begin, begin + classes)));
		if (best != previous && best != 0) {
			text += best == characters.size() + 1 ? " " : characters.at(best - 1);
		}
		previous = best;
	}
	return text;
}

std::string assembleText(const std::vector<Box> &boxes, const std::vector<std::string> &parts)
{
	struct Row
	{
		float centerY;
		float height;
		std::string text;
	};
	std::vector<Row> rows;
	for (size_t index = 0; index < boxes.size(); ++index) {
		const auto centerY =
			(boxes[index][0].y + boxes[index][1].y + boxes[index][2].y + boxes[index][3].y) / 4.0f;
		const auto height = std::max(static_cast<float>(cv::norm(boxes[index][0] - boxes[index][3])), 1.0f);
		if (rows.empty() || std::abs(centerY - rows.back().centerY) > std::max(height, rows.back().height) * 0.5f) {
			rows.push_back({centerY, height, parts[index]});
			continue;
		}
		if (!rows.back().text.empty() && rows.back().text.back() != ' ' &&
			!parts[index].empty() && parts[index].front() != ' ') {
			rows.back().text += ' ';
		}
		rows.back().text += parts[index];
	}
	std::string result;
	for (const auto &row : rows) {
		if (!result.empty()) {
			result += '\n';
		}
		result += row.text;
	}
	return result;
}

std::vector<std::string> dictionary(const QResource &resource)
{
	const auto *start = reinterpret_cast<const char *>(resourceData(resource));
	const auto *end = start + resource.size();
	const auto *item = start;
	std::vector<std::string> result;
	for (const auto *cursor = start; cursor < end; ++cursor) {
		if (*cursor == '\0') {
			result.emplace_back(item, cursor);
			item = cursor + 1;
		}
	}
	return result;
}

} // namespace

class PaddleOcrRecognizer::Implementation
{
public:
	Implementation() :
		mDetectorResource(QStringLiteral(":/ocr/det.onnx")),
		mRecognizerResource(QStringLiteral(":/ocr/rec.onnx")),
		mDictionaryResource(QStringLiteral(":/ocr/dictionary.bin")),
		mEnvironment(ORT_LOGGING_LEVEL_ERROR, "ksnip-ocr"),
		mDetector(mEnvironment, resourceData(mDetectorResource), static_cast<size_t>(mDetectorResource.size())),
		mRecognizer(mEnvironment, resourceData(mRecognizerResource), static_cast<size_t>(mRecognizerResource.size())),
		mCharacters(dictionary(mDictionaryResource))
	{
	}

	std::string recognize(const cv::Mat &image)
	{
		auto detectorOutput = mDetector.run(preprocessDetector(image));
		auto boxes = detectorBoxes(detectorOutput, image.cols, image.rows);
		std::vector<std::string> parts;
		parts.reserve(boxes.size());
		for (const auto &box : boxes) {
			auto crop = cropBox(image, box);
			auto part = decode(mRecognizer.run(preprocessRecognizer(crop)), mCharacters);
			parts.push_back(std::move(part));
		}
		return assembleText(boxes, parts);
	}

private:
	QResource mDetectorResource;
	QResource mRecognizerResource;
	QResource mDictionaryResource;
	Ort::Env mEnvironment;
	InferenceModel mDetector;
	InferenceModel mRecognizer;
	std::vector<std::string> mCharacters;
};

PaddleOcrRecognizer::PaddleOcrRecognizer() = default;

PaddleOcrRecognizer::~PaddleOcrRecognizer() = default;

OcrResult PaddleOcrRecognizer::recognize(const QImage &image)
{
	std::lock_guard<std::mutex> lock(mMutex);
	try {
		if (!mImplementation) {
			mImplementation = std::make_unique<Implementation>();
		}
		auto text = mImplementation->recognize(toBgrImage(image));
		return {QString::fromUtf8(text.data(), static_cast<int>(text.size())), {}};
	} catch (const std::exception &exception) {
		qWarning() << "Built-in OCR recognition failed:" << exception.what();
		return {{}, QStringLiteral("Recognition failed")};
	} catch (...) {
		qWarning() << "Built-in OCR recognition failed with an unknown error";
		return {{}, QStringLiteral("Recognition failed")};
	}
}
