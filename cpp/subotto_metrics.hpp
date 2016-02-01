#ifndef SUBOTTO_METRICS_HPP_
#define SUBOTTO_METRICS_HPP_

#include <opencv2/core/core.hpp>

using namespace cv;

struct SubottoMetrics {
	float length = 1.15;
	float width = 0.70;
};

struct SubottoReferenceMetrics {
  Point2f red_defence_corner;
  Point2f red_attack_corner;
  Point2f blue_defence_corner;
  Point2f blue_attack_corner;

  SubottoReferenceMetrics();
  Size get_ideal_rectangle_size(float alpha=1.0);
};

cv::Mat unitsToSize(SubottoMetrics metrics, cv::Size size);
cv::Mat sizeToUnits(SubottoMetrics metrics, cv::Size size);

Mat referenceToSize(SubottoReferenceMetrics metrics, SubottoMetrics table_metrics);
Mat sizeToReference(SubottoReferenceMetrics metrics, SubottoMetrics table_metrics);

Mat sizeToSize(Size from, Size to);

std::vector<cv::Point2f> subottoCornersUnits(SubottoMetrics metrics);
std::vector<cv::Point2f> subottoCornersMeters(SubottoMetrics metrics);
std::vector<cv::Point2f> subottoReferenceCorners(SubottoReferenceMetrics metrics);
std::vector<cv::Point2f> sizeCorners(cv::Size metrics);

#endif /* SUBOTTO_METRICS_HPP_ */
