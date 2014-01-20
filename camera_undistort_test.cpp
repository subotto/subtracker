#include <iostream>
#include <string>

#include "subotto_tracking.hpp"
#include "utility.hpp"

using namespace cv;
using namespace std;

VideoCapture cap;
SubottoMetrics metrics;
SubottoReference reference;
SubottoTrackingParams trackingParams;

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

	SubottoTracker tracker(cap, reference, metrics, trackingParams);

	Trackbar<float> fx("camera", "fx", 160.f, -1000.f, 1000.f, 0.1f);
	Trackbar<float> fy("camera", "fy", 160.f, -1000.f, 1000.f, 0.1f);
	Trackbar<float> cx("camera", "cx", 160.f, -1000.f, 1000.f, 0.1f);
	Trackbar<float> cy("camera", "cy", 120.f, -1000.f, 1000.f, 0.1f);

	Trackbar<float> k1("camera", "k1", -0.040f, -0.1f, 0.1f, 0.0001f);
	Trackbar<float> k2("camera", "k2", 0.f, -0.1f, 0.1f, 0.0001f);
	Trackbar<float> p1("camera", "p1", 0.f, -0.1f, 0.1f, 0.0001f);
	Trackbar<float> p2("camera", "p2", 0.f, -0.1f, 0.1f, 0.0001f);
	Trackbar<float> k3("camera", "k3", 0.f, -0.1f, 0.1f, 0.0001f);

	bool play = false;
	while (true) {
		if(waitKey(play) == ' ') {
			play = !play;
		}

		Matx<float, 3, 3> cameraMatrix(fx.get(), 0, cx.get(), 0, fy.get(), cy.get(), 0, 0, 1);

		auto tracking = tracker.next();

		Mat frame = tracking.frame;
		Mat undistorted;

		vector<double> params {
			k1.get(), k2.get(), p1.get(), p2.get(), k3.get() };

		vector<double> rparams {
			-k1.get(), -k2.get(), -p1.get(), -p2.get(), -k3.get() };

		cout << InputArray(params).getMat() << endl;

		undistort(frame, undistorted, cameraMatrix, params);

		drawSubottoBorders(undistorted, tracking.transform, Scalar(1.f, 0.f, 0.f));

		Mat reverted;
		undistort(undistorted, reverted, getDefaultNewCameraMatrix(cameraMatrix), rparams, cameraMatrix);

		show("frame", frame);
		show("undistorted", undistorted);
		show("reverted", reverted);
	}
	return 0;
}
