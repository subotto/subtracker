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

vector<Point2f> subottoReferenceCorners(SubottoReferenceMetrics metrics) {
	float dx = metrics.offset.x;
	float dy = metrics.offset.y;
	float w = metrics.frameSize.width;
	float h = metrics.frameSize.height;

	return {
		Point2f(dx+w, dy+h),
		Point2f(dx+w, dy-h),
		Point2f(dx-w, dy-h),
		Point2f(dx-w, dy+h)
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

Mat referenceToSize(SubottoReferenceMetrics metrics, Size size) {
	Mat transform;
	getPerspectiveTransform(subottoReferenceCorners(metrics), sizeCorners(size)).convertTo(transform, CV_32F);
	return transform;
}

Mat sizeToReference(SubottoReferenceMetrics metrics, Size size) {
	Mat transform;
	getPerspectiveTransform(sizeCorners(size), subottoReferenceCorners(metrics)).convertTo(transform, CV_32F);
	return transform;
}
