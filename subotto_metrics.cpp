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
    metrics.blue_defence_corner,
    metrics.red_attack_corner,
    metrics.red_defence_corner,
    metrics.blue_attack_corner,
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

SubottoReferenceMetrics::SubottoReferenceMetrics()
  : red_defence_corner(252, 72),
    red_attack_corner(67, 67),
    blue_defence_corner(58, 183),
    blue_attack_corner(253, 186) {

}
