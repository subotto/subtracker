#include "ball_density.hpp"

#include <iostream>
#include <unordered_map>
#include <opencv2/video/video.hpp>

using namespace std;
using namespace cv;

unordered_map<string, unique_ptr<int>> intensities;

static void show(string name, Mat image, int initialAlpha = 1000) {
	if(intensities.find(name) == intensities.end()) {
		intensities[name] = unique_ptr<int>(new int(initialAlpha));
	}

	float alpha = *intensities[name] / 1000.f;
	Mat output = image * alpha + 0.5 * (1 - alpha);

	namedWindow("intensities", CV_WINDOW_NORMAL);
	namedWindow(name, CV_WINDOW_NORMAL);

	createTrackbar(name, "intensities", intensities[name].get(), 100000);

	imshow(name, output);
}

const Size size(240, 120);

BallDensityEstimator::BallDensityEstimator(unique_ptr<SubottoTracker> subottoTracker)
:
	subottoTracker(move(subottoTracker))
{
}

BallDensityEstimator::~BallDensityEstimator()
{
}

Mat BallDensityEstimator::getInput(const BallDensity& density) {
	Mat frame;
	density.tracking.frame.convertTo(frame, CV_32F, 1 / 255.f);
	Mat warpTransform = density.tracking.transform * getCSTInv(size);
	Mat input;
	warpPerspective(frame, input, warpTransform, size, CV_WARP_INVERSE_MAP);
	return input;
}

int tableMeanFrameAlphaInt = 10;

int tableVarianceDiff2AlphaInt = 10;
int tableVarianceBordersAlphaInt = 30; // was 50
int tableVarianceGammaInt = 20;

int tableMeanLaplacianSize = 2;
int tableDiffLowFilterSize = 80;

int ballColorValueInt = 85;
int ballColorValueVarianceInt = 190;

int tableProbThresholdInt = 6000;

int expShift = 1000;

BallDensity BallDensityEstimator::next() {
	BallDensity density;

	namedWindow("slides", CV_WINDOW_NORMAL);

	createTrackbar("tableMeanFrameAlphaInt", "slides", &tableMeanFrameAlphaInt, 10000);
	createTrackbar("tableVarianceDiff2AlphaInt", "slides", &tableVarianceDiff2AlphaInt, 10000);
	createTrackbar("tableVarianceBordersAlphaInt", "slides", &tableVarianceBordersAlphaInt, 10000);
	createTrackbar("tableVarianceGammaInt", "slides", &tableVarianceGammaInt, 10000);
	createTrackbar("tableMeanLaplacianSize", "slides", &tableMeanLaplacianSize, 10);
	createTrackbar("tableDiffLowFilterSize", "slides", &tableDiffLowFilterSize, 100);
	createTrackbar("ballColorValueInt", "slides", &ballColorValueInt, 100);
	createTrackbar("ballColorValueVarianceInt", "slides", &ballColorValueVarianceInt, 10000);
	createTrackbar("tableProbThresholdInt", "slides", &tableProbThresholdInt, 10000);
	createTrackbar("expShift", "slides", &expShift, 10000);

	density.tracking = subottoTracker->next();

	Mat input = getInput(density);

	if(tableMean.empty()) {
		input.copyTo(tableMean);
		float v = tableVarianceGammaInt / 10000.f;
		Scalar varianceScalar = Scalar(v, v, v);
		tableEstimatedVariance = Mat(tableMean.size(), tableMean.type(), varianceScalar);
	}

	Mat tableDiff = input - tableMean;

	Mat tableDiffLow;
	blur(tableDiff, tableDiffLow, Size(tableDiffLowFilterSize, tableDiffLowFilterSize));
	Mat tableDiffNoLow = tableDiff - tableDiffLow;

	Mat tableDiffNoLow2 = tableDiffNoLow.mul(tableDiffNoLow);

	Mat tableMeanLaplacian, tableMeanBorders;
	Laplacian(tableMean, tableMeanLaplacian, -1, 1 + 2 * tableMeanLaplacianSize);
	tableMeanBorders = tableMeanLaplacian.mul(tableMeanLaplacian);

	Mat tableCorrectedVariance;
	tableEstimatedVariance.copyTo(tableCorrectedVariance);
	accumulateWeighted(tableMeanBorders, tableCorrectedVariance, tableVarianceBordersAlphaInt / 10000.f);
	accumulateWeighted(Mat(tableCorrectedVariance.size(), tableCorrectedVariance.type(), Scalar(1, 1, 1)), tableCorrectedVariance, tableVarianceGammaInt / 10000.f);

	Mat tableDiffNorm = tableDiffNoLow2 / tableCorrectedVariance;

	accumulateWeighted(input, tableMean, tableMeanFrameAlphaInt / 10000.f);
	accumulateWeighted(tableDiffNoLow2, tableEstimatedVariance, tableVarianceDiff2AlphaInt / 10000.f);

	float ballColorValue = ballColorValueInt / 100.f;
	Mat ballDiff = input - Scalar(ballColorValue, ballColorValue, ballColorValue);
	Mat ballDiff2 = ballDiff.mul(ballDiff);

	float ballColorValueVariance = ballColorValueVarianceInt / 10000.f;

	Mat ballDiffNorm = ballDiff2 / Mat(ballDiff2.size(), ballDiff2.type(), Scalar(ballColorValueVariance, ballColorValueVariance, ballColorValueVariance));

	Mat tableProb;
	transform(tableDiffNorm, tableProb, -Matx<float, 1, 3>(1, 1, 1));

	Mat notTableProbTrunc;
	threshold(-tableProb, notTableProbTrunc, tableProbThresholdInt / 100.f, 0, CV_THRESH_TRUNC);
	Mat tableProbTrunc = -notTableProbTrunc;

	Mat ballProb;
	transform(ballDiffNorm, ballProb, -Matx<float, 1, 3>(1, 1, 1));

	Mat pixelProb = ballProb - tableProbTrunc;
	Mat posProb;
	blur(pixelProb, posProb, Size(3, 3));

	double maxPosProb;
	minMaxLoc(posProb, nullptr, &maxPosProb);

	Mat posProbExp;
	exp(posProb - maxPosProb + (expShift / 100.f) - 10, posProbExp);

	show("frame", density.tracking.frame);
	show("input", input);
	show("tableMean", tableMean);
	show("tableMeanBorders", tableMeanBorders);
	show("tableEstimatedVariance", tableEstimatedVariance, 20000);
	show("tableCorrectedVariance", tableCorrectedVariance, 20000);
	show("tableDiff", tableDiff, 3000);
	show("tableDiffLow", tableDiffLow, 3000);
	show("tableDiffNoLow2", tableDiffNoLow2, 20000);
	show("tableDiffNorm", tableDiffNorm, 200);
	show("ballDiff2", ballDiff2, 20000);
	show("ballDiffNorm", ballDiffNorm, 200);
	show("tableProb", tableProb, 50);
	show("ballProb", ballProb, 50);
	show("tableProbTrunc", tableProbTrunc, 50);
	show("posProb", posProb, 5);
	show("posProbExp", posProbExp);

	return density;
}
