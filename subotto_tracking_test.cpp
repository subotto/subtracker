#include <iostream>
#include <string>

#include "subotto_tracking.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/photo/photo.hpp>
#include <opencv2/videostab/videostab.hpp>

using namespace cv;
using namespace std;

VideoCapture cap;

shared_ptr<SubottoTrackingParams> trackerParams(new SubottoTrackingParams());
unique_ptr<SubottoTracker> tracker;
SubottoReference reference;

void onChange(int a, void* b) {
	tracker = unique_ptr<SubottoTracker>(new SubottoTracker(cap, reference, trackerParams));
}

int main(int argc, char* argv[]) {
	string videoName, referenceImageName, referenceImageMaskName;
	if (argc == 3 || argc == 4) {
		videoName = argv[1];
		referenceImageName = argv[2];

		if(argc == 4) {
			referenceImageMaskName = argv[3];
		}
	} else {
		cerr << "Usage: " << argv[0] << " <video> <reference subotto> [<reference subotto mask>]" << endl;
		return 1;
	}

	cap.open(videoName);

	reference.image = imread(referenceImageName);

	if(!referenceImageMaskName.empty()) {
		reference.mask = imread(referenceImageMaskName, CV_LOAD_IMAGE_GRAYSCALE);
	}

	auto& detectorParams = trackerParams->detectionParams;
	auto& followerParams = trackerParams->followingParams;

	onChange(0, 0);

	namedWindow("slidesDetection");
	namedWindow("slides");

	createTrackbar("referenceFeatures", "slidesDetection", &detectorParams.referenceDetection.features, 2000, onChange);
	createTrackbar("referenceLevels", "slidesDetection", &detectorParams.referenceDetection.levels, 10, onChange);

	createTrackbar("preliminaryFrameFeatures", "slidesDetection", &detectorParams.coarseMatching.detection.features, 2000, onChange);
	createTrackbar("preliminaryFrameLevels", "slidesDetection", &detectorParams.coarseMatching.detection.levels, 10, onChange);

	createTrackbar("preliminaryRansacThreshold", "slidesDetection", &detectorParams.coarseRansacThreshold, 2000, onChange);
	createTrackbar("preliminaryRansacOutliersRatio", "slidesDetection", &detectorParams.coarseRansacOutliersRatio, 100, onChange);

	createTrackbar("secondaryFrameFeatures", "slidesDetection", &detectorParams.fineMatching.detection.features, 2000, onChange);
	createTrackbar("secondaryFrameLevels", "slidesDetection", &detectorParams.fineMatching.detection.levels, 10, onChange);

	createTrackbar("secondaryRansacThreshold", "slidesDetection", &detectorParams.fineRansacThreshold, 2000, onChange);

	createTrackbar("followFeatures", "slides", &followerParams.opticalFlow.detection.features, 2000, onChange);
	createTrackbar("followRansacThreshold", "slides", &followerParams.ransacThreshold, 2000, onChange);

	createTrackbar("detectionSkipFrames", "slides", &trackerParams->detectionSkipFrames, 1000, onChange);
	createTrackbar("detectionAlpha", "slides", &trackerParams->detectionAlpha, 100, onChange);

	createTrackbar("followingSkipFrames", "slides", &trackerParams->followingSkipFrames, 1000, onChange);
	createTrackbar("followingAlpha", "slides", &trackerParams->followingAlpha, 100, onChange);

	createTrackbar("previousTransformSmoothingAlpha", "slides", &trackerParams->nearTransformSmoothingAlpha, 100, onChange);

	const Size size(400, 240);

	Mat mean;
	Mat variance;
	while (waitKey(1)) {
		Point2f subottoPoints[] = {
				Point2f(-1, -0.6),
				Point2f(-1, +0.6),
				Point2f(+1, +0.6),
				Point2f(+1, -0.6),
		};

		Point2f imagePoints[] = {
				Point2f(0, 0),
				Point2f(0, size.height),
				Point2f(size.width, size.height),
				Point2f(size.width, 0)
		};

		Mat unzoom;
		getPerspectiveTransform(imagePoints, subottoPoints).convertTo(unzoom, CV_32F);

		auto subottoTracking = tracker->next();

		Mat frame = subottoTracking->frame;

		Mat warpedByte;
		warpPerspective(frame, warpedByte, subottoTracking->transform * unzoom, size, CV_WARP_INVERSE_MAP);

		Mat warped;
		warpedByte.convertTo(warped, CV_32F, 1 / 255.f);

		namedWindow("warped", CV_WINDOW_NORMAL);
		imshow("warped", warped);

		namedWindow("warped", CV_WINDOW_NORMAL);
		imshow("warped", warped);
	}

	return 0;
}
