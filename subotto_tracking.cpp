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

PointMap opticalFlow(FeatureDetectionResult features, Mat referenceImage,
		Mat image) {
	SparsePyrLkOptFlowEstimator ofe;

	vector<Point_<float>> from, to;
	for (KeyPoint keyPoint : features.keyPoints) {
		from.push_back(keyPoint.pt);
	}

	vector<uchar> status;
	ofe.run(referenceImage, image, from, to, status, noArray());

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

Mat detect_table(Mat frame, Mat reference_image, Mat reference_mask, SubottoDetectorParams params) {
	PyramidAdaptedFeatureDetector coarse_fd(new GoodFeaturesToTrackDetector(300), 3);
	BriefDescriptorExtractor de;

	vector<KeyPoint> frame_features;
	coarse_fd.detect(frame, frame_features, Mat());

	Mat frame_features_descriptions;
	de.compute(frame, frame_features, frame_features_descriptions);

	vector<KeyPoint> reference_features;
	coarse_fd.detect(reference_image, reference_features, reference_mask);

	Mat reference_features_descriptions;
	de.compute(reference_image, reference_features, reference_features_descriptions);

	vector<vector<DMatch>> matches_groups;

	BFMatcher dm;
	dm.knnMatch(reference_features_descriptions, frame_features_descriptions, matches_groups, params.coarseMatching.knn, Mat());

	float ransacThreshold = params.coarseRansacThreshold / 100.f;
	float ransacOutliersRatio = params.coarseRansacOutliersRatio / 100.f;

	vector<Point2f> coarse_from, coarse_to;

	for (auto matches : matches_groups) {
		for (DMatch match : matches) {
			auto f = reference_features[match.queryIdx].pt;
			auto t = frame_features[match.trainIdx].pt;

			coarse_from.push_back(f);
			coarse_to.push_back(t);
		}
	}

	RansacParams ransac_params(6, ransacThreshold, ransacOutliersRatio, 0.99f);
	float rmse;
	int ninliers;
	Mat coarse_transform = estimateGlobalMotionRobust(coarse_from, coarse_to, LINEAR_SIMILARITY, ransac_params, &rmse, &ninliers);

	cerr << "subotto detect - rmse: " << rmse << " inliers: " << ninliers << "/" << coarse_from.size() << endl;

	Mat warped;
	warpPerspective(frame, warped, coarse_transform, reference_image.size(), WARP_INVERSE_MAP | INTER_LINEAR);

	show("subotto phase 1", warped);

	vector<KeyPoint> optical_flow_features;

	GoodFeaturesToTrackDetector(300).detect(warped, optical_flow_features);

	SparsePyrLkOptFlowEstimator ofe;

	vector<Point2f> optical_flow_from, optical_flow_to;
	vector<uchar> status;

	for(KeyPoint kp : optical_flow_features) {
		optical_flow_from.push_back(kp.pt);
	}

	ofe.run(reference_image, warped, optical_flow_from, optical_flow_to, status, noArray());

	vector<Point2f> good_optical_flow_from, good_optical_flow_to;

	for (int i = 0; i < optical_flow_from.size(); i++) {
		if (!status[i]) {
			continue;
		}

		good_optical_flow_from.push_back(optical_flow_from[i]);
		good_optical_flow_to.push_back(optical_flow_to[i]);
	}

	Mat flow_correction;
	if (good_optical_flow_from.size() < 6) {
		flow_correction = Mat::eye(3, 3, CV_32F);
	} else {
		float ransac_threshold = params.flowRansacThreshold / 100.f;
		findHomography(good_optical_flow_from, good_optical_flow_to, RANSAC, ransac_threshold).convertTo(
				flow_correction, CV_32F);
	}

	Mat flow_transform = coarse_transform * flow_correction;

	Mat transform = flow_transform * referenceToSize(params.metrics, reference_image.size());

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
		frameReader(frameReader), params(params), reference(reference), follower(
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
		Mat detectionTransform = detect_table(frame, reference.image, reference.mask, params.detectionParams);
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
