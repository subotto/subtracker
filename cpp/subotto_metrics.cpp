#include "subotto_metrics.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <vector>

using namespace cv;
using namespace std;

vector<Point2f> subottoCornersUnits(SubottoMetrics metrics) {
	float ratio = metrics.width / metrics.length;

	return {
		Point2f(+1, +ratio),
		Point2f(+1, -ratio),
		Point2f(-1, -ratio),
		Point2f(-1, +ratio)
	};
}

vector<Point2f> subottoCornersMeters(SubottoMetrics metrics) {
	float w = metrics.width;
	float l = metrics.length;

	return {
		Point2f(+l, +w),
		Point2f(+l, -w),
		Point2f(-l, -w),
		Point2f(-l, -w)
	};
}

vector<Point2f> sizeCorners(Size size) {
	float w = size.width;
	float h = size.height;

	return {
		Point2f(w, h),
		Point2f(w, 0),
		Point2f(0, 0),
		Point2f(0, h)
	};
}

vector< Point2f > subottoReferenceMetricsCorners(SubottoReferenceMetrics metrics) {

  return {
    metrics.red_attack_corner,
    metrics.blue_defence_corner,
    metrics.blue_attack_corner,
    metrics.red_defence_corner,
  };

}

Mat unitsToSize(SubottoMetrics metrics, Size size) {
	Mat transform;
	getPerspectiveTransform(subottoCornersUnits(metrics), sizeCorners(size)).convertTo(transform, CV_32F);
	return transform;
}

Mat sizeToUnits(SubottoMetrics metrics, Size size) {
	Mat transform;
	getPerspectiveTransform(sizeCorners(size), subottoCornersUnits(metrics)).convertTo(transform, CV_32F);
	return transform;
}

Mat referenceToSize(SubottoReferenceMetrics metrics, SubottoMetrics table_metrics) {
	Mat transform;
  getPerspectiveTransform(subottoCornersUnits(table_metrics), subottoReferenceMetricsCorners(metrics)).convertTo(transform, CV_32F);
	return transform;
}

Mat sizeToReference(SubottoReferenceMetrics metrics, SubottoMetrics table_metrics) {
	Mat transform;
  getPerspectiveTransform(subottoReferenceMetricsCorners(metrics), subottoCornersUnits(table_metrics)).convertTo(transform, CV_32F);
	return transform;
}

Mat sizeToSize(Size from, Size to) {
	Mat transform;
  getPerspectiveTransform(sizeCorners(from), sizeCorners(to)).convertTo(transform, CV_32F);
	return transform;
}

SubottoReferenceMetrics::SubottoReferenceMetrics()
  : red_defence_corner(131, 364),
    red_attack_corner(497, 339),
    blue_defence_corner(482, 116),
    blue_attack_corner(121, 141) {
  /*  : red_defence_corner(130, 379),
    red_attack_corner(527, 384),
    blue_defence_corner(526, 160),
    blue_attack_corner(163, 154) {*/

}

inline static double avg(double a, double b) {

  return (a+b)/2;

}

Size SubottoReferenceMetrics::get_ideal_rectangle_size(float alpha) {

  float alpha_root = sqrt(alpha);
  return Size(alpha_root * avg(norm(red_defence_corner - red_attack_corner), norm(blue_defence_corner - blue_attack_corner)),
              alpha_root * avg(norm(red_defence_corner - blue_attack_corner), norm(red_attack_corner - blue_defence_corner)));

}
