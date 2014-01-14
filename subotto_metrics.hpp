#ifndef SUBOTTO_METRICS_HPP_
#define SUBOTTO_METRICS_HPP_

#include <opencv2/core/core.hpp>

struct SubottoMetrics {
	float length = 1.15;
	float width = 0.70;
};

struct SubottoReferenceMetrics {
	cv::Point2f offset = cv::Point2f(0.01, -0.02);
	cv::Size2f frameSize = cv::Size2f(1.55, 0.75);
};

cv::Mat unitsToSize(SubottoMetrics metrics, cv::Size size);
cv::Mat sizeToUnits(SubottoMetrics metrics, cv::Size size);

cv::Mat referenceToSize(SubottoReferenceMetrics metrics, cv::Size size);
cv::Mat sizeToReference(SubottoReferenceMetrics metrics, cv::Size size);

std::vector<cv::Point2f> subottoCornersUnits(SubottoMetrics metrics);
std::vector<cv::Point2f> subottoCornersMeters(SubottoMetrics metrics);
std::vector<cv::Point2f> subottoReferenceCorners(SubottoReferenceMetrics metrics);
std::vector<cv::Point2f> sizeCorners(cv::Size metrics);

#endif /* SUBOTTO_METRICS_HPP_ */
