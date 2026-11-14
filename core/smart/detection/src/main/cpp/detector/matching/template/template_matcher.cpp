/*
 * Copyright (C) 2025 Kevin Buzeau
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/imgproc_c.h>

#include "template_matcher.hpp"
#include "../../../logs/log.h"
#include "../../../utils/roi.h"


using namespace smartautoclicker;


void TemplateMatcher::reset() {
    currentMatchingResult.reset();
}

TemplateMatchingResult *TemplateMatcher::getMatchingResults() {
    return &currentMatchingResult;
}

bool TemplateMatcher::isRoiValidForMatching(const cv::Rect& screenRoi, const cv::Rect& conditionRoi, const cv::Rect& roi) {
    if (!isRoiBiggerOrEquals(screenRoi, conditionRoi)) {
        LOGD("Detector", "Can't detectCondition, condition (w=%d, h=%d) is bigger than screen (w=%d, h=%d)",
             conditionRoi.width, conditionRoi.height, screenRoi.width, screenRoi.height);
        return false;
    }

    if (roi.width <= 0 || roi.height <= 0 || !isRoiContainsOrEquals(screenRoi, roi)) {
        LOGD("Detector", "Can't detectCondition, detection area (x=%d, y=%d, w=%d, h=%d) is not contained in screen (w=%d, h=%d)",
             roi.x, roi.y, roi.width, roi.height,
             screenRoi.width, screenRoi.height);
        return false;
    }

    if (!isRoiBiggerOrEquals(roi, conditionRoi)) {
        LOGD("Detector", "Can't detectCondition, condition (w=%d, h=%d) is bigger than detection area (x=%d, y=%d, w=%d, h=%d)",
             conditionRoi.width, conditionRoi.height, roi.x, roi.y, roi.width, roi.height);
        return false;
    }

    return true;
}

void TemplateMatcher::matchTemplate(
        const ScreenImage& screenImage,
        const ConditionImage& condition,
        const cv::Rect& detectionArea,
        int threshold
) {

    // Keep the screen crop, condition template and correlation output in
    // UMat objects. OpenCV selects its OpenCL matchTemplate implementation
    // when the library was built with WITH_OPENCL and the device is available.
    cv::UMat screenCroppedGray = screenImage.cropGrayUMat(detectionArea);
    if (screenCroppedGray.empty()) {
        LOGE("TemplateMatcher", "screenCroppedGray is empty after cropping.");
        return;
    }

    const cv::UMat& conditionGray = condition.getGrayUMat();
    if (conditionGray.empty()) {
        LOGE("TemplateMatcher", "conditionGray is empty.");
        return;
    }

    cv::UMat newResultsUMat;

    try {
        cv::matchTemplate(
                screenCroppedGray,
                conditionGray,
                newResultsUMat,
                cv::TM_CCOEFF_NORMED);
    } catch (const cv::Exception& e) {
        LOGE("TemplateMatcher", "OpenCV Exception caught: %s", e.what());
        throw;
    } catch (const std::exception& e) {
        LOGE("TemplateMatcher", "Standard Exception caught: %s", e.what());
        throw;
    } catch (...) {
        LOGE("TemplateMatcher", "Unknown exception caught!");
        throw std::runtime_error("Unknown exception in TemplateMatcher");
    }

    // Keep the existing result validation/suppression logic. Mapping the
    // correlation result back to Mat is only needed for CPU-side result parsing.
    cv::Mat newResultsMat = newResultsUMat.getMat(cv::ACCESS_READ);
    parseMatchingResult(screenImage, condition, detectionArea, threshold, newResultsMat);
}

void TemplateMatcher::parseMatchingResult(
        const ScreenImage& screenImage,
        const ConditionImage& condition,
        const cv::Rect& detectionArea,
        int threshold,
        cv::Mat& matchingResult
) {

    while (!currentMatchingResult.isDetected()) {

        // Mark previous results as invalid, if any
        if (!currentMatchingResult.getResultArea().empty()) {
            currentMatchingResult.invalidateCurrentResult(
                    condition.getGrayMat(),
                    matchingResult);
        }

        // Look for new best match
        currentMatchingResult.updateResults(
                detectionArea,
                condition.getGrayMat(),
                matchingResult);

        // Check if the highest result is above threshold. If not, we will never find.
        if (!isConfidenceValid(currentMatchingResult.getResultConfidence(), threshold)) break;

        // Check if result area is valid. If not, check next possible match
        if (!isRoiBiggerOrEquals(screenImage.getRoi(), currentMatchingResult.getResultArea())) continue;

        // Check if the colors are matching in the candidate area.
        cv::Mat hsvCrop = screenImage.cropHsv(currentMatchingResult.getResultArea());
        double colorDiff = getColorDiff(hsvCrop, condition.getHsvMean());

        // If the colors are OK, the result is valid
        if (colorDiff <= threshold) currentMatchingResult.markResultAsDetected();
    }
}

bool TemplateMatcher::isConfidenceValid(double confidence, int threshold) {
    return confidence > ((100.0 - threshold) / 100.0);
}

double TemplateMatcher::getColorDiff(const cv::Mat& hsvImage, const cv::Scalar& conditionHsvMean) {
    cv::Scalar imageHsvMean = cv::mean(hsvImage);

    // Compute shortest arc distance (H channel is circular [0, 180] in OpenCV)
    double hDiff = std::abs(imageHsvMean.val[0] - conditionHsvMean.val[0]);
    if (hDiff > 90.0) hDiff = 180.0 - hDiff;

    // S and V channels are linear [0, 255]
    double sDiff = std::abs(imageHsvMean.val[1] - conditionHsvMean.val[1]);
    double vDiff = std::abs(imageHsvMean.val[2] - conditionHsvMean.val[2]);

    // Normalize each channel to [0, 100] then average
    return ((hDiff / 90.0) + (sDiff / 255.0) + (vDiff / 255.0)) * (100.0 / 3.0);
}
