#include "SubottoDetector.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/videostab/videostab.hpp>

#include <iostream>

using namespace std;
using namespace cv;
using namespace cv::videostab;

SubottoDetector::SubottoDetector(VideoCapture cap, Mat referenceImage,
		Mat referenceImageMask, shared_ptr<SubottoDetectorParams> params) :
		cap(cap), referenceImage(referenceImage), referenceImageMask(
				referenceImageMask), params(params), hasPreviousFrame(false) {
}

SubottoDetector::~SubottoDetector() {
}

Mat SubottoDetector::changeCoordinateSystem(const Mat& estimatedTransform) {
	Point_<float> referenceImagePoints[] = { Point_<float>(0, 0), Point_<float>(
			referenceImage.cols, 0), Point_<float>(0, referenceImage.rows) };
	Point_<float> subottoPoints[] = { Point_<float>(-1, -0.6), Point_<float>(+1,
			-0.6), Point_<float>(-1, +0.6) };
	Mat coordinateSystemTransformDbl = getAffineTransform(subottoPoints,
			referenceImagePoints);
	Mat coordinateSystemTransform;

	coordinateSystemTransformDbl.convertTo(coordinateSystemTransform, CV_32F);
	Mat row = (Mat_<float>(1, 3) << 0, 0, 1);

	coordinateSystemTransform.push_back(row);
	Mat subottoTransform = estimatedTransform * coordinateSystemTransform;
	return subottoTransform;
}

Point_<float> transform(Point_<float> p, Mat m) {
	Mat pm = (Mat_<float>(3, 1) << p.x, p.y, 1);
	Mat tpm = m * pm;
	float w = tpm.at<float>(2, 0);
	return Point_<float>(tpm.at<float>(0, 0) / w, tpm.at<float>(1, 0) / w);
}

struct FeatureMatchingParams {
	int referenceFeatures;
	int referenceLevels;
	int frameFeatures;
	int frameLevels;
};

void extractFeaturePoints(Mat referenceImage, Mat mask, Mat frame,
		vector<Point_<float>>& referencePoints,
		vector<Point_<float>>& framePoints, FeatureMatchingParams params) {
	unique_ptr<DescriptorExtractor> descriptorExtractor(
			new BriefDescriptorExtractor());
	unique_ptr<DescriptorMatcher> descriptorMatcher(
			new BFMatcher(NORM_HAMMING));

	unique_ptr<FeatureDetector> referenceFeatureDetector(
			new PyramidAdaptedFeatureDetector(
					new GoodFeaturesToTrackDetector(params.referenceFeatures, 0.0001, 0),
					params.referenceLevels));
	unique_ptr<FeatureDetector> frameFeatureDetector(
			new PyramidAdaptedFeatureDetector(
					new GoodFeaturesToTrackDetector(params.frameFeatures, 0.0001, 0),
					params.frameLevels));

	vector<KeyPoint> referenceKeyPoints, frameKeyPoints;
	Mat referenceDescriptors, frameDescriptors;

	referenceFeatureDetector->detect(referenceImage, referenceKeyPoints, mask);
	descriptorExtractor->compute(referenceImage, referenceKeyPoints,
			referenceDescriptors);

	frameFeatureDetector->detect(frame, frameKeyPoints);
	descriptorExtractor->compute(frame, frameKeyPoints, frameDescriptors);

	vector<DMatch> matches;
	descriptorMatcher->match(referenceDescriptors, frameDescriptors, matches);

	for (DMatch match : matches) {
		referencePoints.push_back(referenceKeyPoints[match.queryIdx].pt);
		framePoints.push_back(frameKeyPoints[match.trainIdx].pt);
	}

	namedWindow("matches", CV_WINDOW_NORMAL);
	Mat output;
	drawMatches(referenceImage, referenceKeyPoints, frame, frameKeyPoints,
			matches, output);
	imshow("matches", output);
}

struct OpticalFlowParams {
	int referenceFeatures = 300;
};

void extractOpticalFlowPoints(Mat referenceImage, Mat mask, Mat frame,
		vector<Point_<float>>& referencePoints,
		vector<Point_<float>>& framePoints, OpticalFlowParams const& params) {
	unique_ptr<FeatureDetector> featureDetector(
			new GoodFeaturesToTrackDetector(params.referenceFeatures, 0.0001, 0));
	unique_ptr<ISparseOptFlowEstimator> optFlowEstimator(
			new SparsePyrLkOptFlowEstimator());

	vector<KeyPoint> keyPoints;
	featureDetector->detect(referenceImage, keyPoints, mask);

	vector<Point_<float>> points1, points2;
	for (KeyPoint keyPoint : keyPoints) {
		points1.push_back(keyPoint.pt);
	}

	if (points1.empty()) {
		return;
	}

	vector<uchar> status;
	optFlowEstimator->run(referenceImage, frame, points1, points2, status, noArray());

	for(int i = 0; i < points1.size(); i++) {
		if(!status[i]) {
			continue;
		}

		referencePoints.push_back(points1[i]);
		framePoints.push_back(points2[i]);
	}
}

void show(string name, Mat image) {
	namedWindow(name, CV_WINDOW_NORMAL);
	imshow(name, image);
}

Mat SubottoDetector::computePreliminaryTransform(const Mat& frame) {
	vector<Point_<float>> referencePoints, framePoints;

	FeatureMatchingParams matchingParams;
	matchingParams.referenceFeatures = params->preliminaryReferenceFeatures;
	matchingParams.referenceLevels = params->preliminaryReferenceLevels;
	matchingParams.frameFeatures = params->preliminaryFrameFeatures;
	matchingParams.frameLevels = params->preliminaryFrameLevels;

	extractFeaturePoints(referenceImage, referenceImageMask, frame,
			referencePoints, framePoints, matchingParams);

	RansacParams ransacParams = RansacParams(6,
			params->preliminaryRansacThreshold / 100.f,
			params->preliminaryRansacOutliersRatio / 100.f, 0.99);

	return estimateGlobalMotionRobust(referencePoints, framePoints,
			LINEAR_SIMILARITY, ransacParams);
}

Mat SubottoDetector::computeSecondaryCorrection(const Mat& frame) {
	vector<Point_<float>> referencePoints, framePoints;

	FeatureMatchingParams matchingParams;
	matchingParams.referenceFeatures = params->secondaryReferenceFeatures;
	matchingParams.referenceLevels = params->secondaryReferenceLevels;
	matchingParams.frameFeatures = params->secondaryFrameFeatures;
	matchingParams.frameLevels = params->secondaryFrameLevels;

	extractFeaturePoints(referenceImage, referenceImageMask, frame,
			referencePoints, framePoints, matchingParams);

	if(referencePoints.size() < 6) {
		return Mat::eye(3, 3, CV_32F);
	}

	Mat secondaryCorrection;
	findHomography(referencePoints, framePoints, RANSAC,
			params->secondaryRansacThreshold / 100.f).convertTo(
			secondaryCorrection, CV_32F);
	return secondaryCorrection;
}

Mat SubottoDetector::applySecondaryCorrection(const Mat& transform,
		const Mat& frame) {
	Mat transformInv;
	invert(transform, transformInv);

	Size size = Size(referenceImage.cols, referenceImage.rows);
	Mat warpedFrame;

	warpPerspective(frame, warpedFrame, transformInv, size);
	show("beforeSecondaryCorrection", warpedFrame);

	Mat secondaryCorrection = computeSecondaryCorrection(warpedFrame);
	Mat secondaryTransform = secondaryCorrection * transform;

	return secondaryTransform;
}

Mat SubottoDetector::computeOpticalFlowCorrection(const Mat& frame) {
	vector<Point_<float>> referencePoints, framePoints;

	OpticalFlowParams optFlowParams;
	optFlowParams.referenceFeatures = 100;
	extractOpticalFlowPoints(referenceImage, referenceImageMask, frame,
			referencePoints, framePoints,
			optFlowParams);

	if(referencePoints.size() < 6) {
		return Mat::eye(3, 3, CV_32F);
	}

	Mat optFlowCorrection;
	findHomography(referencePoints, framePoints, RANSAC).convertTo(
			optFlowCorrection, CV_32F);

	return optFlowCorrection;
}

Mat SubottoDetector::applyOpticalFlowCorrection(const Mat& transform,
		const Mat& frame) {
	Mat transformInv;
	invert(transform, transformInv);

	Size size = Size(referenceImage.cols, referenceImage.rows);
	Mat warpedFrame;

	warpPerspective(frame, warpedFrame, transformInv, size);
	show("beforeOpticalFlowCorrection", warpedFrame);

	Mat opticalFlowCorrection = computeOpticalFlowCorrection(warpedFrame);
	Mat estimatedTransform = opticalFlowCorrection * transform;

	return estimatedTransform;
}

Mat SubottoDetector::stabilize(const Mat& frame, const Mat& transform) {
	if(!hasPreviousFrame) {
		Mat transformCopy;
		transform.copyTo(transformCopy);
		return transformCopy;
	}

	Mat stabilizedTransform;

	Mat previousTransformInv;
	invert(previousTransform, previousTransformInv);

	Size size = Size(referenceImage.cols, referenceImage.rows);
	Mat previousWarpedFrame, currentWarpedFrame;
	warpPerspective(frame, currentWarpedFrame, previousTransformInv, size);
	warpPerspective(previousFrame, previousWarpedFrame, previousTransformInv, size);

	vector<Point_<float>> previousPoints, currentPoints;
	OpticalFlowParams opticalFlowParams;
	opticalFlowParams.referenceFeatures = params->stabReferenceFeatures;
	extractOpticalFlowPoints(previousWarpedFrame, Mat(), currentWarpedFrame,
			previousPoints, currentPoints, opticalFlowParams);

	Mat motion;

	imshow("stabilizePrevious", previousWarpedFrame);
	imshow("stabilizeCurrent", currentWarpedFrame);

	if(previousPoints.size() >= 6) {
		// motion = estimateGlobalMotionRobust(previousPoints, currentPoints, AFFINE);
		findHomography(previousPoints, currentPoints, RANSAC).convertTo(motion, CV_32F);
	} else {
		motion = Mat::eye(3, 3, CV_32F);
	}

	Mat movedTransform = motion * previousTransform;

	double alpha = 0.05;
	addWeighted(movedTransform, (1-alpha), transform, alpha, 0,
			stabilizedTransform);

	return stabilizedTransform;
}

unique_ptr<SubottoInfo> SubottoDetector::next() {
	Mat frame;
	cap >> frame;

	unique_ptr<SubottoInfo> subottoInfo(new SubottoInfo);

	subottoInfo->frame = frame;

	show("frame", frame);

	Mat subottoTransform;

	Mat preliminaryTransform = computePreliminaryTransform(frame);
	Mat secondaryTransform = applySecondaryCorrection(preliminaryTransform, frame);
	Mat estimatedTransform = applyOpticalFlowCorrection(secondaryTransform, frame);
	subottoTransform = stabilize(frame, estimatedTransform);

	frame.copyTo(previousFrame);
	subottoTransform.copyTo(previousTransform);
	hasPreviousFrame = true;

	subottoInfo->subottoTransform = changeCoordinateSystem(subottoTransform);

	return subottoInfo;
}

void drawSubottoBorders(Mat& outImage, SubottoInfo const& info, Scalar color) {
	vector<Point_<float>> subottoCorners = { Point_<float>(-1, -0.6), Point_<
			float>(-1, +0.6), Point_<float>(+1, +0.6), Point_<float>(+1, -0.6) };

	for (int corner = 0; corner < subottoCorners.size(); corner++) {
		subottoCorners[corner] = transform(subottoCorners[corner],
				info.subottoTransform);
	}

	for (int corner = 0; corner < subottoCorners.size(); corner++) {
		line(outImage, subottoCorners[corner],
				subottoCorners[(corner + 1) % subottoCorners.size()], color);
	}
}
