#include <iostream>
#include <string>

#include "SubottoDetector.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/photo/photo.hpp>
#include <opencv2/videostab/videostab.hpp>

using namespace cv;
using namespace std;

int main(int argc, char* argv[]) {
	string videoName, referenceImageName, referenceImageMaskName;
	if (argc == 4) {
		videoName = argv[1];
		referenceImageName = argv[2];
		referenceImageMaskName = argv[3];
	} else {
		cerr << "Usage: " << argv[0] << " <video> <reference subotto> <reference subotto mask>" << endl;
		return 1;
	}

	VideoCapture cap;
	cap.open(videoName);
	Mat referenceImage = imread(referenceImageName);
	Mat referenceImageMask = imread(referenceImageMaskName, CV_LOAD_IMAGE_GRAYSCALE);

	std::shared_ptr<SubottoDetectorParams> params(new SubottoDetectorParams());

	SubottoDetector sd(cap, referenceImage, referenceImageMask, params);

	namedWindow("output1", CV_WINDOW_NORMAL);
	namedWindow("output2", CV_WINDOW_NORMAL);
	namedWindow("output3", CV_WINDOW_NORMAL);
	namedWindow("output4", CV_WINDOW_NORMAL);

	namedWindow("slides");

	createTrackbar("preliminaryReferenceFeatures", "slides", &params->preliminaryReferenceFeatures, 2000);
	createTrackbar("preliminaryReferenceLevels", "slides", &params->preliminaryReferenceLevels, 10);
	createTrackbar("preliminaryFrameFeatures", "slides", &params->preliminaryFrameFeatures, 2000);
	createTrackbar("preliminaryFrameLevels", "slides", &params->preliminaryFrameLevels, 10);

	createTrackbar("preliminaryRansacThreshold", "slides", &params->preliminaryRansacThreshold, 2000);
	createTrackbar("preliminaryRansacOutliersRatio", "slides", &params->preliminaryRansacOutliersRatio, 100);

	createTrackbar("secondaryReferenceFeatures", "slides", &params->secondaryReferenceFeatures, 2000);
	createTrackbar("secondaryReferenceLevels", "slides", &params->secondaryReferenceLevels, 10);
	createTrackbar("secondaryFrameFeatures", "slides", &params->secondaryFrameFeatures, 2000);
	createTrackbar("secondaryFrameLevels", "slides", &params->secondaryFrameLevels, 10);

	createTrackbar("secondaryRansacThreshold", "slides", &params->secondaryRansacThreshold, 2000);

	createTrackbar("flowReferenceFeatures", "slides", &params->flowReferenceFeatures, 2000);
	createTrackbar("stabReferenceFeatures", "slides", &params->stabReferenceFeatures, 2000);
	createTrackbar("ransacReprojThreshold", "slides", &params->ransacReprojThreshold, 5000);

	Mat output1, output2, output3, output4;

	const Size size(200, 120);

	bool inited = false;
	Mat avgBackground;

	while(waitKey(1)) {
		auto subottoInfo = sd.next();

		Mat frame = subottoInfo->frame;
		Mat warpedFrame;

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

		Mat unzoomDbl = getPerspectiveTransform(subottoPoints, imagePoints);
		Mat unzoom;
		unzoomDbl.convertTo(unzoom, CV_32F);

		Mat subottoTransformInv;
		invert(subottoInfo->subottoTransform, subottoTransformInv);
		warpPerspective(frame, warpedFrame, unzoom * subottoTransformInv, size);

		if(!inited) {
			warpedFrame.copyTo(avgBackground);
			inited = true;
		}
		Mat newAvgBackground;

		Mat backgroundAddendum;

		GaussianBlur(warpedFrame, backgroundAddendum, Size(5,5), 2, 1);

		const double alpha = 0.02;
		addWeighted(backgroundAddendum, alpha, avgBackground, (1 - alpha), 0, newAvgBackground);
		avgBackground = newAvgBackground;

		Mat warpedFrameFlt, avgBackgroundFlt;

		warpedFrame.convertTo(warpedFrameFlt, CV_32F);
		avgBackground.convertTo(avgBackgroundFlt, CV_32F);

		Mat difference = warpedFrameFlt - avgBackgroundFlt;
		Mat squareDifference = difference.mul(difference) / 20000.f;

		frame.copyTo(output1);
		drawSubottoBorders(output1, *subottoInfo, Scalar(0, 255, 0));

		output2 = warpedFrame;
		output3 = avgBackground;
		output4 = squareDifference;

		imshow("output1", output1);
		imshow("output2", output2);
		imshow("output3", output3);
		imshow("output4", output4);

		Mat output;
		squareDifference.convertTo(output, CV_8UC4);
	}

	return 0;
}
