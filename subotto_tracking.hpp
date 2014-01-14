#ifndef SUBOTTODETECTOR_H_
#define SUBOTTODETECTOR_H_

#include "subotto_metrics.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/videostab/videostab.hpp>

#include <memory>

struct SubottoReference {
	cv::Mat image;
	cv::Mat mask;
	SubottoReferenceMetrics metrics;
};

struct FeatureDetectionParams {
	int features = 300;
	int levels = 3;
};

struct FeatureDetectionResult {
	std::vector<cv::KeyPoint> keyPoints;
	cv::Mat descriptors;
};

struct FeatureMatchingParams {
	FeatureDetectionParams detection;
};

struct PointMap {
	std::vector<cv::Point_<float>> from;
	std::vector<cv::Point_<float>> to;
};

struct OpticalFlowParams {
	FeatureDetectionParams detection;
};

struct SubottoDetectorParams {
	FeatureDetectionParams referenceDetection;

	FeatureMatchingParams coarseMatching;
	int coarseRansacThreshold = 2000;
	int coarseRansacOutliersRatio = 60;

	FeatureMatchingParams fineMatching;
	int fineRansacThreshold = 200;
};

struct SubottoFollowingParams {
	OpticalFlowParams opticalFlow;
	int ransacThreshold = 200;

	SubottoFollowingParams() {
		opticalFlow.detection.features = 20;
	}
};

struct SubottoTrackingParams {
	SubottoDetectorParams detectionParams;
	SubottoFollowingParams followingParams;

	int detectionSkipFrames = 60;
	int detectionAlpha = 10;

	int followingSkipFrames = 0;
	int followingAlpha = 25;

	int nearTransformSmoothingAlpha = 20;
};

struct SubottoTracking {
	cv::Mat frame;
	cv::Mat transform;
};

/*
 * Finds the Subotto in a given frame, given a reference image of the Subotto (possibly masked).
 * Has no memory of previous frames.
 * The detection operation is slow, and cannot be run online for every frame.
 *
 * It uses a pipeline of three steps:
 * 	1) Coarse features matching
 * 	2) Fine features matching
 *
 * Coarse features matching:
 * Detects the coarse position of the Subotto in the frame, matching a few features.
 * The output of this phase is a linear similarity (translation, rotation and isotropic zoom)
 * which maps roughly the reference image to the frame.
 *
 * Fine features matching:
 * Improves the position of the Subotto and introduces perspective corrections, matching more features.
 * This phase takes in input the frame warped according to the previous transform
 * and outputs a perspective transform.
 */

class SubottoDetector {
public:
	SubottoDetector(SubottoReference reference, SubottoMetrics metrics, SubottoDetectorParams params);
	virtual ~SubottoDetector();

	cv::Mat detect(cv::Mat frame);
private:
	SubottoReference reference;
	SubottoDetectorParams params;
	SubottoMetrics metrics;

	FeatureDetectionResult referenceFeatures;
};

/*
 * Improves the position of the Subotto by estimating the optical flow
 * from the reference image to the given frame.
 * It needs an initial transform which should be a good approximation of the real transform.
 * Indeed, it takes advantage from the locality of the optical flow.
 * This operations should be fast enough to be run (almost) online.
 */
class SubottoFollower {
public:
	SubottoFollower(SubottoReference reference, SubottoMetrics metrics, SubottoFollowingParams params);
	virtual ~SubottoFollower();

	cv::Mat follow(cv::Mat frame, cv::Mat initialTransform);
private:
	SubottoReference reference;
	SubottoFollowingParams params;
	SubottoMetrics metrics;

	FeatureDetectionResult referenceFeatures;
};

/*
 * Combines a SubottoDetector and a SubottoFollower to track the position of the subotto over time.
 * It invokes the SubottoDetector only once in a while, ad uses the SubottoFollower to
 * track the subotto (almost) frame to frame.
 */
class SubottoTracker {
public:
	SubottoTracker(cv::VideoCapture cap, SubottoReference reference, SubottoMetrics metrics, SubottoTrackingParams params);
	virtual ~SubottoTracker();

	SubottoTracking next();
private:
	cv::VideoCapture cap;
	SubottoReference reference;
	SubottoMetrics metrics;
	SubottoTrackingParams params;

	std::unique_ptr<SubottoDetector> detector;
	std::unique_ptr<SubottoFollower> follower;

	cv::Mat previousTransform;
	cv::Mat nearTransform;

	int frameCount = 0;
};

void drawSubottoBorders(cv::Mat& outImage, const cv::Mat& transform, cv::Scalar color);

#endif /* SUBOTTODETECTOR_H_ */
