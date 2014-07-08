#ifndef SUBOTTO_METRICS_HPP_
#define SUBOTTO_METRICS_HPP_

#include <opencv2/core/core.hpp>

struct SubottoMetrics {
	float length = 1.15;
	float width = 0.70;
};

/*struct SubottoReferenceMetrics {
	cv::Point2f offset {-0.016f, -0.018f};
	cv::Size2f frameSize {-1.98, -0.98};
  };*/

struct SubottoReferenceMetrics {
	cv::Point2f offset {0.0f, 0.0f};
	cv::Size2f frameSize {-1.67, -1.25};
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
