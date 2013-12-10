#include <iostream>
#include <string>

#include "ball_density.hpp"

using namespace cv;
using namespace std;

VideoCapture cap;

unique_ptr<BallDensityEstimator> densityEstimator;
SubottoReference reference;

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

	unique_ptr<SubottoTracker> subottoTracker(new SubottoTracker(cap, reference, SubottoTrackingParams()));
	densityEstimator = unique_ptr<BallDensityEstimator>(new BallDensityEstimator(move(subottoTracker)));

	bool play = false;
	while (true) {
		if(waitKey(play) == ' ') {
			play = !play;
		}

		auto ballDensity = densityEstimator->next();
	}
	return 0;
}
