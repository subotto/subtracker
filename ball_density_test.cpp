#include <iostream>
#include <string>

#include "ball_density.hpp"
#include "utility.hpp"

using namespace cv;
using namespace std;

VideoCapture cap;

unique_ptr<BallDensityEstimator> densityEstimator;
SubottoReference reference;
SubottoMetrics metrics;

BallDensityEstimatorParams params;

void onChange(int a, void* b) {
	unique_ptr<SubottoTracker> subottoTracker(new SubottoTracker(cap, reference, metrics, SubottoTrackingParams()));
	densityEstimator = unique_ptr<BallDensityEstimator>(new BallDensityEstimator(move(subottoTracker), params));
}

int expShift = 500;

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

	namedWindow("slides", CV_WINDOW_NORMAL);

	createTrackbar("expShift", "slides", &expShift, 10000);
	createTrackbar("tableMeanFrameAlphaInt", "slides", &params.tableMeanFrameAlphaInt, 10000, onChange);
	createTrackbar("tableVarianceDiff2AlphaInt", "slides", &params.tableVarianceDiff2AlphaInt, 10000, onChange);
	createTrackbar("tableVarianceBordersAlphaInt", "slides", &params.tableVarianceBordersAlphaInt, 10000, onChange);
	createTrackbar("tableVarianceGammaInt", "slides", &params.tableVarianceGammaInt, 10000, onChange);
	createTrackbar("tableMeanLaplacianSize", "slides", &params.tableMeanLaplacianSize, 10, onChange);
	createTrackbar("tableDiffLowFilterSize", "slides", &params.tableDiffLowFilterStdDev, 100, onChange);
	createTrackbar("ballColorValueInt", "slides", &params.ballColorValueInt, 100, onChange);
	createTrackbar("ballColorValueVarianceInt", "slides", &params.ballColorValueVarianceInt, 10000, onChange);
	createTrackbar("tableProbThresholdInt", "slides", &params.tableProbThresholdInt, 10000, onChange);

	onChange(0, 0);

	bool play = false;
	while (true) {
		if(waitKey(play) == ' ') {
			play = !play;
		}

		auto density = densityEstimator->next();
		auto i = densityEstimator->getLastInternals();

		show("frame", density.tracking.frame);
		show("input", i.input);
		show("tableMean", i.tableMean);
		show("tableMeanBorders", i.tableMeanBorders);
		show("tableEstimatedVariance", i.tableEstimatedVariance, 20000);
		show("tableCorrectedVariance", i.tableCorrectedVariance, 20000);
		show("logTableCorrectedVariance", i.logTableEstimatedVariance, 20000);
		show("tableDiff", i.tableDiff, 3000, 50);
		show("tableDiffLow", i.tableDiffLow, 3000, 50);
		show("tableDiffNoLow2", i.tableDiffNoLow2, 20000);
		show("tableDiffNorm", i.tableDiffNorm, 200, 50);
		show("ballDiff2", i.ballDiff2, 20000);
		show("ballDiffNorm", i.ballDiffNorm, 50, 50);
		show("tableProb", i.tableProb, 50);
		show("ballProb", i.ballProb, 50);
		show("tableProbTrunc", i.tableProbTrunc, 50);
		show("posProb", i.posProb, 5);

		double maxPosProb;
		minMaxLoc(i.posProb, nullptr, &maxPosProb);

		Mat posProbExp;
		exp(i.posProb - maxPosProb + (expShift / 100.f) - 10, posProbExp);

		show("posProbExp", posProbExp);
	}
	return 0;
}
