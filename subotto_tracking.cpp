#include "subotto_tracking.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/videostab/videostab.hpp>

#include <iostream>

using namespace std;
using namespace cv;
using namespace cv::videostab;

const float subottoHalfWidth = 0.5;

unique_ptr<FeatureDetector> createFeatureDetector(
		FeatureDetectionParams params) {
	auto gfttd = new GoodFeaturesToTrackDetector(params.features);
	auto pafd = new PyramidAdaptedFeatureDetector(gfttd, params.levels);
	return unique_ptr<FeatureDetector>(pafd);
}

unique_ptr<DescriptorExtractor> createDescriptorExtractor() {
	auto bde = new BriefDescriptorExtractor();
	return unique_ptr<DescriptorExtractor>(bde);
}

unique_ptr<DescriptorMatcher> createDescriptorMatcher() {
	auto dm = new BFMatcher(NORM_HAMMING);
	return unique_ptr<DescriptorMatcher>(dm);
}

unique_ptr<ISparseOptFlowEstimator> createOpticalFlowEstimator() {
	auto ofe = new SparsePyrLkOptFlowEstimator();
	return unique_ptr<ISparseOptFlowEstimator>(ofe);
}

FeatureDetectionResult detectFeatures(Mat image, Mat mask,
		FeatureDetectionParams params) {
	auto fd = createFeatureDetector(params);
	auto de = createDescriptorExtractor();

	FeatureDetectionResult result;

	fd->detect(image, result.keyPoints, mask);
	de->compute(image, result.keyPoints, result.descriptors);

	return result;
}

PointMap matchFeatures(FeatureDetectionResult features, Mat image,
		FeatureMatchingParams params) {
	auto imageFeatures = detectFeatures(image, Mat(), params.detection);
	auto dm = createDescriptorMatcher();

	vector<DMatch> matches;
	dm->match(features.descriptors, imageFeatures.descriptors, matches);

	PointMap map;
	for (DMatch match : matches) {
		auto from = features.keyPoints[match.queryIdx].pt;
		auto to = imageFeatures.keyPoints[match.trainIdx].pt;

		map.from.push_back(from);
		map.to.push_back(to);
	}

	return map;
}

PointMap opticalFlow(FeatureDetectionResult features, Mat referenceImage,
		Mat image) {
	auto ofe = createOpticalFlowEstimator();

	vector<Point_<float>> from, to;
	for (KeyPoint keyPoint : features.keyPoints) {
		from.push_back(keyPoint.pt);
	}

	vector<uchar> status;
	ofe->run(referenceImage, image, from, to, status, noArray());

	PointMap map;
	for (int i = 0; i < from.size(); i++) {
		if (!status[i]) {
			continue;
		}

		map.from.push_back(from[i]);
		map.to.push_back(to[i]);
	}

	return map;
}

vector<Point_<float>> getSubottoCorners() {
	return {
		Point_<float>(+1, +subottoHalfWidth),
		Point_<float>(+1, -subottoHalfWidth),
		Point_<float>(-1, -subottoHalfWidth),
		Point_<float>(-1, +subottoHalfWidth)
	};
}

vector<Point_<float>> getCorners(Size size) {
	return {
		Point_<float>(size.width, size.height),
		Point_<float>(size.width, 0),
		Point_<float>(0, 0),
		Point_<float>(0, size.height)
	};
}

// cordinate system transform
Mat getCST(Size fromSize) {
	Mat coordinateSystemTransform;
	getPerspectiveTransform(getSubottoCorners(), getCorners(fromSize)).convertTo(
			coordinateSystemTransform, CV_32F);
	return coordinateSystemTransform;
}

Mat getCSTInv(Size fromSize) {
	Mat coordinateSystemTransform;
	getPerspectiveTransform(getCorners(fromSize), getSubottoCorners()).convertTo(
			coordinateSystemTransform, CV_32F);
	return coordinateSystemTransform;
}

Point_<float> applyTransform(Point_<float> p, Mat m) {
	Mat pm = (Mat_<float>(3, 1) << p.x, p.y, 1);
	Mat tpm = m * pm;
	float w = tpm.at<float>(2, 0);
	return Point_<float>(tpm.at<float>(0, 0) / w, tpm.at<float>(1, 0) / w);
}

static void show(string name, Mat image) {
	namedWindow(name, CV_WINDOW_NORMAL);
	imshow(name, image);
}

void showWarpedSubotto(string windowName, Mat frame, Mat transform) {
	Size size = Size(400, 240);

	Mat imageTransform = transform * getCSTInv(size);

	Mat warpedFrame;
	warpPerspective(frame, warpedFrame, imageTransform, size,
			CV_WARP_INVERSE_MAP);
	show(windowName, warpedFrame);
}

SubottoDetector::SubottoDetector(SubottoReference reference,
		SubottoDetectorParams params) :
		reference(reference), params(params), referenceFeatures(
				detectFeatures(reference.image, reference.mask,
						params.referenceDetection)) {
}

SubottoDetector::~SubottoDetector() {
}

Mat SubottoDetector::detect(Mat frame) {
	PointMap coarseMap = matchFeatures(referenceFeatures, frame,
			params.coarseMatching);

	float ransacThreshold = params.coarseRansacThreshold / 100.f;
	float ransacOutliersRatio = params.coarseRansacOutliersRatio / 100.f;

	RansacParams ransacParams(4, ransacThreshold, ransacOutliersRatio, 0.99f);
	Mat coarseTransform = estimateGlobalMotionRobust(coarseMap.from,
			coarseMap.to, LINEAR_SIMILARITY, ransacParams);

	Mat warped;
	Size size(reference.image.cols, reference.image.rows);
	warpPerspective(frame, warped, coarseTransform, size, CV_WARP_INVERSE_MAP);

	PointMap fineMap = matchFeatures(referenceFeatures, warped,
			params.fineMatching);
	float fineRansacThreshold = params.fineRansacThreshold / 100.f;
	Mat fineCorrection;
	findHomography(fineMap.from, fineMap.to, RANSAC, fineRansacThreshold).convertTo(
			fineCorrection, CV_32F);

	Mat transform = coarseTransform * fineCorrection * getCST(size);

	return transform;
}

SubottoFollower::SubottoFollower(SubottoReference reference,
		SubottoFollowingParams params) :
		reference(reference), params(params), referenceFeatures(
				detectFeatures(reference.image, reference.mask,
						params.opticalFlow.detection)) {
}

SubottoFollower::~SubottoFollower() {
}

Mat SubottoFollower::follow(Mat frame, Mat previousTransform) {
	Mat warped;
	Size size(reference.image.cols, reference.image.rows);

	warpPerspective(frame, warped, previousTransform * getCSTInv(size), size,
			CV_WARP_INVERSE_MAP);

	PointMap map = opticalFlow(referenceFeatures, reference.image, warped);

	Mat correction;
	if (map.from.size() < 6) {
		correction = Mat::eye(3, 3, CV_32F);
	} else {
		float ransacThreshold = params.ransacThreshold / 100.f;
		findHomography(map.from, map.to, RANSAC, ransacThreshold).convertTo(
				correction, CV_32F);
	}

	return previousTransform * getCSTInv(size) * correction * getCST(size);
}

SubottoTracker::SubottoTracker(VideoCapture cap, SubottoReference reference,
		SubottoTrackingParams params) :
		cap(cap), params(params), reference(reference), detector(
				new SubottoDetector(reference, params.detectionParams)), follower(
				new SubottoFollower(reference, params.followingParams)) {
}

SubottoTracker::~SubottoTracker() {
}

SubottoTracking SubottoTracker::next() {
	SubottoTracking subottoTracking;
	Mat frame;

	cap >> frame;

	Mat subottoTransform;

	bool shouldFollow = frameCount % (params.followingSkipFrames + 1) == 0;
	bool shouldDetect = frameCount % (params.detectionSkipFrames + 1) == 0;

	if(!previousTransform.empty()) {
		previousTransform.copyTo(subottoTransform);
	}

	if (shouldFollow && !nearTransform.empty()) {
		Mat followingTransform = follower->follow(frame, nearTransform);

		if (subottoTransform.empty()) {
			followingTransform.copyTo(subottoTransform);
		} else {
			accumulateWeighted(followingTransform, subottoTransform,
					params.followingAlpha / 100.f);
		}
	}

	Mat followDetectedTransform;
	if (shouldDetect || subottoTransform.empty()) {
		Mat detectionTransform = detector->detect(frame);
		followDetectedTransform = follower->follow(frame, detectionTransform);

		if (subottoTransform.empty()) {
			followDetectedTransform.copyTo(subottoTransform);
		} else {
			accumulateWeighted(followDetectedTransform, subottoTransform,
					params.detectionAlpha / 100.f);
		}
	}

	if (nearTransform.empty()) {
		subottoTransform.copyTo(nearTransform);
	} else {
		float alpha = params.nearTransformSmoothingAlpha / 100.f;
		accumulateWeighted(subottoTransform, nearTransform, alpha);
	}

	frameCount++;

	subottoTracking.frame = frame;
	subottoTracking.transform = subottoTransform;

	subottoTransform.copyTo(previousTransform);

	return subottoTracking;
}

void drawSubottoBorders(Mat& outImage, const Mat& transform, Scalar color) {
	vector<Point_<float>> subottoCorners = { Point_<float>(-1, -0.6), Point_<
			float>(-1, +0.6), Point_<float>(+1, +0.6), Point_<float>(+1, -0.6) };

	for (int corner = 0; corner < subottoCorners.size(); corner++) {
		subottoCorners[corner] = applyTransform(subottoCorners[corner],
				transform);
	}

	for (int corner = 0; corner < subottoCorners.size(); corner++) {
		line(outImage, subottoCorners[corner],
				subottoCorners[(corner + 1) % subottoCorners.size()], color);
	}
}
