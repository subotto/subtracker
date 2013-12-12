#include "ball_density.hpp"

#include <iostream>
#include <unordered_map>
#include <opencv2/video/video.hpp>

using namespace std;
using namespace cv;

unordered_map<string, unique_ptr<int>> showAlpha, showGamma;

void show(string name, Mat image, int initialAlpha, int initialGamma) {
	if(showAlpha.find(name) == showAlpha.end()) {
		showAlpha[name] = unique_ptr<int>(new int(initialAlpha));
		showGamma[name] = unique_ptr<int>(new int(initialGamma));
	}

	float alpha = *showAlpha[name] / 1000.f;
	float gamma = *showGamma[name] / 100.f;
	Mat output = image * alpha + Scalar(gamma, gamma, gamma);

	namedWindow("intensities", CV_WINDOW_NORMAL);
	namedWindow(name, CV_WINDOW_NORMAL);

	createTrackbar(name + "Alpha", "intensities", showAlpha[name].get(), 100000);
	createTrackbar(name + "Gamma", "intensities", showGamma[name].get(), 100);

	imshow(name, output);
}

const Size size(240, 120);

BallDensityEstimator::BallDensityEstimator(unique_ptr<SubottoTracker> subottoTracker, BallDensityEstimatorParams params)
:
	subottoTracker(move(subottoTracker)),
	params(params)
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

BallDensity BallDensityEstimator::next() {
	BallDensity density;

	density.tracking = subottoTracker->next();

	BallDensityEstimatorInternals& i = lastInternals;

	i.input = getInput(density);

	Size size = i.input.size();
	int type = i.input.type();

	if(tableMean.empty()) {
		i.input.copyTo(tableMean);
		float v = params.tableVarianceGammaInt / 10000.f;
		Scalar varianceScalar = Scalar(v, v, v);
		tableEstimatedVariance = Mat(size, type, varianceScalar);
	}

	i.tableDiff = i.input - tableMean;

	blur(i.tableDiff, i.tableDiffLow, Size(params.tableDiffLowFilterSize, params.tableDiffLowFilterSize));
	i.tableDiffNoLow = i.tableDiff - i.tableDiffLow;
	i.tableDiffNoLow2 = i.tableDiffNoLow.mul(i.tableDiffNoLow);

	Laplacian(tableMean, i.tableMeanLaplacian, -1, 1 + 2 * params.tableMeanLaplacianSize);
	i.tableMeanBorders = i.tableMeanLaplacian.mul(i.tableMeanLaplacian);

	tableEstimatedVariance.copyTo(i.tableCorrectedVariance);

	accumulateWeighted(i.tableMeanBorders, i.tableCorrectedVariance, params.tableVarianceBordersAlphaInt / 10000.f);
	accumulateWeighted(Mat(size, type, Scalar(1, 1, 1)), i.tableCorrectedVariance, params.tableVarianceGammaInt / 10000.f);

	i.tableDiffNorm = i.tableDiffNoLow2 / i.tableCorrectedVariance;

	accumulateWeighted(i.input, tableMean, params.tableMeanFrameAlphaInt / 10000.f);
	accumulateWeighted(i.tableDiffNoLow2, tableEstimatedVariance, params.tableVarianceDiff2AlphaInt / 10000.f);

	float ballColorValue = params.ballColorValueInt / 100.f;
	i.ballDiff = i.input - Scalar(ballColorValue, ballColorValue, ballColorValue);
	i.ballDiff2 = i.ballDiff.mul(i.ballDiff);

	float ballColorValueVariance = params.ballColorValueVarianceInt / 10000.f;

	i.ballDiffNorm = i.ballDiff2 / Mat(size, type, Scalar(ballColorValueVariance, ballColorValueVariance, ballColorValueVariance));

	i.tableProb;
	transform(i.tableDiffNorm, i.tableProb, -Matx<float, 1, 3>(1, 1, 1));

	i.notTableProbTrunc;
	threshold(-i.tableProb, i.notTableProbTrunc, params.tableProbThresholdInt / 100.f, 0, CV_THRESH_TRUNC);
	i.tableProbTrunc = -i.notTableProbTrunc;

	i.ballProb;
	transform(i.ballDiffNorm, i.ballProb, -Matx<float, 1, 3>(1, 1, 1));

	i.pixelProb = i.ballProb - i.tableProbTrunc;
	i.posProb;
	blur(i.pixelProb, i.posProb, Size(3, 3));

	tableEstimatedVariance.copyTo(i.tableEstimatedVariance);
	tableMean.copyTo(i.tableMean);

//	double maxProb;
//	minMaxLoc(i.posProb, nullptr, &maxProb);

	density.density = i.posProb;

	return density;
}

BallDensityEstimatorInternals BallDensityEstimator::getLastInternals() {
	return lastInternals;
}
