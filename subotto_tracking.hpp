#ifndef SUBOTTODETECTOR_H_
#define SUBOTTODETECTOR_H_

#include "subotto_metrics.hpp"
#include "framereader.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/videostab/videostab.hpp>

#include <memory>

struct SubottoReference {
	cv::Mat image;
	cv::Mat mask;
	SubottoReferenceMetrics metrics;
};

struct FeatureDetectionParams {
	int features;
	int levels;
};

struct FeatureDetectionResult {
	std::vector<cv::KeyPoint> keyPoints;
	cv::Mat descriptors;
};

struct FeatureMatchingParams {
	FeatureDetectionParams detection;
	int knn;
};

struct PointMap {
	std::vector<cv::Point_<float>> from;
	std::vector<cv::Point_<float>> to;
};

struct OpticalFlowParams {
	FeatureDetectionParams detection;
};

struct SubottoDetectorParams {
	FeatureDetectionParams referenceDetection {50, 3};

	FeatureMatchingParams coarseMatching { {1000, 3}, 10 };
	int coarseRansacThreshold = 2000;
	int coarseRansacOutliersRatio = 50;

	OpticalFlowParams opticalFlow { {100, 3} };
	int flowRansacThreshold = 100;

	SubottoReferenceMetrics metrics;
};

struct SubottoFollowingParams {
	OpticalFlowParams opticalFlow { {100, 1} };
	int ransacThreshold = 100;

	cv::Size opticalFlowSize {128, 64};

	SubottoReferenceMetrics metrics;
};

struct SubottoTrackingParams {
	SubottoDetectorParams detectionParams;
	SubottoFollowingParams followingParams;

	int detectionSkipFrames = 120;
	int detectionAlpha = 100;

	int followingSkipFrames = 0;
	int followingAlpha = 100;

	int nearTransformSmoothingAlpha = 25;

	SubottoTrackingParams() {}
};

struct SubottoTracking {
	cv::Mat frame;
	cv::Mat transform;
	frame_info frameInfo;
};

/*
 * Combines a SubottoDetector and a SubottoFollower to track the position of the subotto over time.
 * It invokes the SubottoDetector only once in a while, ad uses the SubottoFollower to
 * track the subotto (almost) frame to frame.
 */
class SubottoTracker {
public:
	SubottoTracker(FrameReader& frameReader, SubottoReference reference, SubottoMetrics metrics, SubottoTrackingParams params);
	virtual ~SubottoTracker();

	SubottoTracking next();
private:
	FrameReader& frameReader;
	SubottoReference reference;
	SubottoMetrics metrics;
	SubottoTrackingParams params;

	cv::Mat previousTransform;
	cv::Mat nearTransform;

	int frameCount = 0;
};

void drawSubottoBorders(cv::Mat& outImage, const cv::Mat& transform, cv::Scalar color);

#endif /* SUBOTTODETECTOR_H_ */
