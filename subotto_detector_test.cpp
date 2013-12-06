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

	SubottoDetector sd(cap, referenceImage, referenceImageMask);

	namedWindow("output1", CV_WINDOW_NORMAL);
	namedWindow("output2", CV_WINDOW_NORMAL);
	namedWindow("output3", CV_WINDOW_NORMAL);
	namedWindow("output4", CV_WINDOW_NORMAL);

	Mat output1, output2, output3, output4;

	const int size = 320;

	bool inited = false;
	Mat avgBackground;

	VideoWriter w;
	w.open("./output.avi", CV_FOURCC_DEFAULT, 120, Size(320, 240), true);

    if (!w.isOpened())
    {
        cout << "Could not open the output video for write: " << endl;
        return -1;
    }

	while(waitKey(1) != 'e') {
		auto subottoInfo = sd.next();

		Mat frame = subottoInfo->frame;
		Mat warpedFrame;

		Point2f subottoPoints[] = {
				Point2f(-1, -1),
				Point2f(-1, +1),
				Point2f(+1, -1)
		};

		Point2f imagePoints[] = {
				Point2f(0, 0),
				Point2f(0, size),
				Point2f(size, 0)
		};

		Mat unzoomDbl = getAffineTransform(subottoPoints, imagePoints);
		Mat unzoom;
		unzoomDbl.convertTo(unzoom, CV_32F);

		warpAffine(frame, warpedFrame, unzoom * subottoInfo->subottoTransformInv, Size(size, size));

		if(!inited) {
			warpedFrame.copyTo(avgBackground);
			inited = true;
		}
		Mat newAvgBackground;

		const double alpha = 0.02;
		addWeighted(warpedFrame, alpha, avgBackground, (1 - alpha), 0, newAvgBackground);
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

		w.write(output);
	}

	return 0;
}
