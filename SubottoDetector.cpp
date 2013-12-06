#include "SubottoDetector.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/videostab/videostab.hpp>

#include <iostream>

using namespace std;
using namespace cv;
using namespace cv::videostab;

const RansacParams ransacParams = RansacParams(6, 0.95f, 0.6f, 0.99f);

SubottoDetector::SubottoDetector(VideoCapture cap, Mat referenceImage, Mat referenceImageMask)
	:
	cap(cap),
	featureDetector(new GoodFeaturesToTrackDetector(200)),
	descriptorExtractor(new OpponentColorDescriptorExtractor(new BriefDescriptorExtractor())),
	descriptorMatcher(new BFMatcher(NORM_HAMMING)),
	referenceImage(referenceImage),
	referenceImageMask(referenceImageMask),
	hasPreviousFrame(false)
{
	featureDetector->detect(referenceImage, referenceKeyPoints, referenceImageMask);
	descriptorExtractor->compute(referenceImage, referenceKeyPoints, referenceDescriptors);
}

SubottoDetector::~SubottoDetector() {
}

Mat SubottoDetector::estimateGlobalMotion(const Mat& frame, vector<KeyPoint>& frameKeyPoints, vector<DMatch>& matches) {
	Mat frameDescriptors;

	featureDetector->detect(frame, frameKeyPoints);
	descriptorExtractor->compute(frame, frameKeyPoints, frameDescriptors);
	descriptorMatcher->match(referenceDescriptors, frameDescriptors, matches);

	vector<Point2f> points1, points2;
	for (DMatch match : matches) {
		points1.push_back(referenceKeyPoints[match.queryIdx].pt);
		points2.push_back(frameKeyPoints[match.trainIdx].pt);
	}

	return estimateGlobalMotionRobust(points1, points2, AFFINE, ransacParams);
}

unique_ptr<SubottoInfo> SubottoDetector::next() {
	const int bufferSize = 20;

	Mat frame;
	cap >> frame;

	unique_ptr<SubottoInfo> subottoInfo(new SubottoInfo);

	subottoInfo->frame = frame;
	subottoInfo->referenceImage = referenceImage;
	subottoInfo->referenceImageKeyPoints = referenceKeyPoints;

	Mat estimatedTransform = estimateGlobalMotion(frame, subottoInfo->frameKeyPoints, subottoInfo->matches);

	if(hasPreviousFrame) {
		Mat previousTransformInv;
		invert(previousSubottoTransform, previousTransformInv);

		Mat previousWarpedFrame;
		Mat warpedFrame;

		Size size = Size(referenceImage.cols, referenceImage.rows);

		warpAffine(previousFrame, previousWarpedFrame, previousTransformInv.rowRange(0, 2), size);
		warpAffine(frame, warpedFrame, previousTransformInv.rowRange(0, 2), size);

		int opticalFlowFeatures = 50;

		PyrLkRobustMotionEstimator me;
		me.setDetector(new GoodFeaturesToTrackDetector(opticalFlowFeatures));
		Mat motion = me.estimate(previousWarpedFrame, warpedFrame);

		Mat motionEstimatedTransform = motion * previousSubottoTransform;

		Mat oldEstimatedTransform = estimatedTransform;

		addWeighted(oldEstimatedTransform, 0.01, motionEstimatedTransform, 0.99, 0, estimatedTransform);
	}

	previousFrame = frame;
	previousSubottoTransform = estimatedTransform;
	hasPreviousFrame = true;

	Point2f referenceImagePoints[] = {
		Point2f(0, 0),
		Point2f(referenceImage.cols, 0),
		Point2f(0, referenceImage.rows)
	};

	Point2f subottoPoints[] = {
		Point2f(-1, -1),
		Point2f(+1, -1),
		Point2f(-1, +1)
	};

	Mat coordinateSystemTransformDbl = getAffineTransform(subottoPoints, referenceImagePoints);
	Mat coordinateSystemTransform;

	coordinateSystemTransformDbl.convertTo(coordinateSystemTransform, CV_32F);

	Mat row = (Mat_<float>(1,3) << 0, 0, 1);
	coordinateSystemTransform.push_back(row);

	subottoInfo->subottoTransform = estimatedTransform * coordinateSystemTransform;
	invert(subottoInfo->subottoTransform, subottoInfo->subottoTransformInv);

	return subottoInfo;
}

void drawSubottoBorders(Mat& outImage, SubottoInfo const& info, Scalar color) {
	vector<Point2f> subottoCorners = {
		Point2f(-1, -1),
		Point2f(-1, +1),
		Point2f(+1, +1),
		Point2f(+1, -1)
	};

	for (int corner = 0; corner < subottoCorners.size(); corner++) {
		Mat point(3, 1, CV_32F);

		point.at<float>(0, 0) = subottoCorners[corner].x;
		point.at<float>(1, 0) = subottoCorners[corner].y;
		point.at<float>(2, 0) = 1;

		Mat tranformedPoint = info.subottoTransform * point;
		subottoCorners[corner].x = tranformedPoint.at<float>(0);
		subottoCorners[corner].y = tranformedPoint.at<float>(1);
	}

	for(int corner = 0; corner < subottoCorners.size(); corner++) {
		line(outImage, subottoCorners[corner], subottoCorners[(corner+1) % subottoCorners.size()], color);
	}
}
