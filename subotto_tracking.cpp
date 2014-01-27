#include "subotto_tracking.hpp"
#include "subotto_metrics.hpp"
#include "utility.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/videostab/videostab.hpp>

#include <iostream>

using namespace std;
using namespace cv;
using namespace cv::videostab;

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

	vector<vector<DMatch>> matchesGroups;
	dm->knnMatch(features.descriptors, imageFeatures.descriptors, matchesGroups, params.knn, Mat());

	PointMap map;
	for (auto matches : matchesGroups) {
		for (DMatch match : matches) {
			auto from = features.keyPoints[match.queryIdx].pt;
			auto to = imageFeatures.keyPoints[match.trainIdx].pt;

			map.from.push_back(from);
			map.to.push_back(to);
		}
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

Point_<float> applyTransform(Point_<float> p, Mat m) {
	Mat pm = (Mat_<float>(3, 1) << p.x, p.y, 1);
	Mat tpm = m * pm;
	float w = tpm.at<float>(2, 0);
	return Point_<float>(tpm.at<float>(0, 0) / w, tpm.at<float>(1, 0) / w);
}

SubottoDetector::SubottoDetector(SubottoReference reference,
		SubottoMetrics metrics,
		SubottoDetectorParams params) :
		reference(reference), metrics(metrics), params(params), referenceFeatures(
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

	Mat coarseTransform;
	if(coarseMap.from.size() < 4) {
		coarseTransform = Mat::eye(3, 3, CV_32F);
	} else {
		RansacParams ransacParams(4, ransacThreshold, ransacOutliersRatio, 0.99f);
		coarseTransform = estimateGlobalMotionRobust(coarseMap.from, coarseMap.to, LINEAR_SIMILARITY, ransacParams);
	}

	Mat warped;
	Size size(reference.image.size());
	warpPerspective(frame, warped, coarseTransform, size, CV_WARP_INVERSE_MAP);

	PointMap fineMap = matchFeatures(referenceFeatures, warped,
			params.fineMatching);
	float fineRansacThreshold = params.fineRansacThreshold / 100.f;

	Mat fineCorrection;
	if(fineMap.from.size() < 6) {
		fineCorrection = Mat::eye(3, 3, CV_32F);
	} else {
		findHomography(fineMap.from, fineMap.to, RANSAC, fineRansacThreshold).convertTo(
				fineCorrection, CV_32F);
	}

	Mat fineTransform = coarseTransform * fineCorrection;
	warpPerspective(frame, warped, fineTransform, size, CV_WARP_INVERSE_MAP);

	PointMap flowMap = opticalFlow(referenceFeatures, reference.image, warped);

	Mat flowCorrection;
	if (flowMap.from.size() < 6) {
		flowCorrection = Mat::eye(3, 3, CV_32F);
	} else {
		float ransacThreshold = params.flowRansacThreshold / 100.f;
		findHomography(flowMap.from, flowMap.to, RANSAC, ransacThreshold).convertTo(
				flowCorrection, CV_32F);
	}

	Mat flowTransform = fineTransform * flowCorrection;

	Mat transform = flowTransform * referenceToSize(reference.metrics, size);

	return transform;
}

SubottoFollower::SubottoFollower(SubottoReference reference,
		SubottoMetrics metrics,
		SubottoFollowingParams params) :
		params(params), metrics(metrics) {
	resize(reference.image, scaledReference.image, params.opticalFlowSize);

	if(!reference.mask.empty()) {
		resize(reference.mask, scaledReference.mask, params.opticalFlowSize);
	}

	scaledReference.metrics = reference.metrics;

	GoodFeaturesToTrackDetector(params.opticalFlow.detection.features).detect(
			scaledReference.image, features, scaledReference.mask);
}

SubottoFollower::~SubottoFollower() {
}

Mat SubottoFollower::follow(Mat frame, Mat previousTransform) {
	Mat warped;
	Size size = scaledReference.image.size();

	warpPerspective(frame, warped, previousTransform * sizeToReference(scaledReference.metrics, size), size,
			WARP_INVERSE_MAP | INTER_LINEAR);

	FeatureDetectionResult featureDetectionResults {features};
	PointMap map = opticalFlow(featureDetectionResults, scaledReference.image, warped);

	Mat correction;
	if (map.from.size() < 6) {
		correction = Mat::eye(3, 3, CV_32F);
	} else {
		float ransacThreshold = params.ransacThreshold / 100.f;
		int ninliers;
		float rmse;
		correction = estimateGlobalMotionRobust(map.from, map.to,
				LINEAR_SIMILARITY,
				RansacParams(6, ransacThreshold, 0.1f, 0.99f), &rmse,
				&ninliers);
	}

	return previousTransform * sizeToReference(scaledReference.metrics, size) * correction * referenceToSize(scaledReference.metrics, size);
}

SubottoTracker::SubottoTracker(FrameReader& frameReader, SubottoReference reference,
		SubottoMetrics metrics,
		SubottoTrackingParams params) :
		frameReader(frameReader), params(params), reference(reference), detector(
				new SubottoDetector(reference, metrics, params.detectionParams)), follower(
				new SubottoFollower(reference, metrics, params.followingParams)) {
}

SubottoTracker::~SubottoTracker() {
}

Mat correctDistortion(Mat frameDistorted) {
//	Matx<float, 3, 3> cameraMatrix(160, 0, 160, 0, 160, 120, 0, 0, 1);
//	float k = -0.025;
//
//	Mat frame;
//	undistort(frameDistorted, frame, cameraMatrix, vector<float> {k, 0, 0, 0}, getDefaultNewCameraMatrix(cameraMatrix));

	return frameDistorted;
}

SubottoTracking SubottoTracker::next() {
	SubottoTracking subottoTracking;

	dumpTime("subotto tracking", "start");

	frame_info frameInfo = frameReader.get();

	dumpTime("subotto tracking", "read frame");

	Mat frameDistorted = frameInfo.data;
	Mat frame = correctDistortion(frameDistorted);

	dumpTime("subotto tracking", "correct distortion");

	Mat subottoTransform;

	bool shouldFollow = frameCount % (params.followingSkipFrames + 1) == 0;
	bool shouldDetect = frameCount % (params.detectionSkipFrames + 1) == 0;

	if(!previousTransform.empty()) {
		previousTransform.copyTo(subottoTransform);
	}

	if (shouldFollow && !nearTransform.empty()) {
		Mat followingTransform = follower->follow(frame, nearTransform);
		dumpTime("subotto tracking", "follow");

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
		dumpTime("subotto tracking", "detect");
		followDetectedTransform = follower->follow(frame, detectionTransform);
		dumpTime("subotto tracking", "follow");

		if (subottoTransform.empty()) {
			followDetectedTransform.copyTo(subottoTransform);
		} else {
			accumulateWeighted(followDetectedTransform, subottoTransform,
					params.detectionAlpha / 100.f);
		}
	}

	dumpTime("subotto tracking", "update transform");

	if (nearTransform.empty() || shouldDetect) {
		subottoTransform.copyTo(nearTransform);
	} else {
		float alpha = params.nearTransformSmoothingAlpha / 100.f;
		accumulateWeighted(subottoTransform, nearTransform, alpha);
	}

	dumpTime("subotto tracking", "update near transform");

	frameCount++;

	subottoTransform.copyTo(previousTransform);

	subottoTracking.frame = frame;
	subottoTracking.transform = subottoTransform;
	subottoTracking.frameInfo = frameInfo;

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
				subottoCorners[(corner + 1) % subottoCorners.size()], color, 1, 16);
	}
}
