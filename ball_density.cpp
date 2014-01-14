#include "ball_density.hpp"

#include <iostream>
#include <unordered_map>
#include <opencv2/video/video.hpp>

using namespace std;
using namespace cv;

const Size size(240, 120);

BallDensityEstimator::BallDensityEstimator(unique_ptr<SubottoTracker> subottoTracker, BallDensityEstimatorParams params)
:
	subottoTracker(move(subottoTracker)),
	params(params),
	metrics()
{
}

BallDensityEstimator::~BallDensityEstimator()
{
}

Mat BallDensityEstimator::getInput(const BallDensity& density) {
	Mat frame;
	density.tracking.frame.convertTo(frame, CV_32F, 1 / 255.f);
	Mat warpTransform = density.tracking.transform * sizeToUnits(metrics, size);
	Mat input;
	warpPerspective(frame, input, warpTransform, size, CV_WARP_INVERSE_MAP | CV_INTER_LINEAR);
	return input;
}

BallDensity BallDensityEstimator::next() {
	BallDensity density;

	density.tracking = subottoTracker->next();

	BallDensityEstimatorInternals& in = lastInternals;

	in.input = getInput(density);

	Size size = in.input.size();
	int type = in.input.type();

	if(tableMean.empty()) {
		in.input.copyTo(tableMean);
		float v = params.tableVarianceGammaInt / 10000.f;
		Scalar varianceScalar = Scalar(v, v, v);
		tableEstimatedVariance = Mat(size, type, varianceScalar);
	}

	in.tableDiff = in.input - tableMean;

	Mat tableDiffLowTemp;
	in.tableDiff.copyTo(tableDiffLowTemp);
	int boxSize = params.tableDiffLowFilterStdDev * 3 * sqrt(2 * CV_PI) / 4 + 0.5;
	for(int i = 0; i < 3; i++) {
		blur(tableDiffLowTemp, in.tableDiffLow, Size(boxSize, boxSize));
		in.tableDiffLow.copyTo(tableDiffLowTemp);
	}

	in.tableDiffNoLow = in.tableDiff - in.tableDiffLow;
	in.tableDiffNoLow2 = in.tableDiffNoLow.mul(in.tableDiffNoLow);

	Laplacian(tableMean, in.tableMeanLaplacian, -1, 1 + 2 * params.tableMeanLaplacianSize);
	in.tableMeanBorders = in.tableMeanLaplacian.mul(in.tableMeanLaplacian);

	tableEstimatedVariance.copyTo(in.tableCorrectedVariance);

	accumulateWeighted(in.tableMeanBorders, in.tableCorrectedVariance, params.tableVarianceBordersAlphaInt / 10000.f);
	accumulateWeighted(Mat(size, type, Scalar(1, 1, 1)), in.tableCorrectedVariance, params.tableVarianceGammaInt / 10000.f);

	in.tableDiffNorm = in.tableDiffNoLow2 / in.tableCorrectedVariance;

	accumulateWeighted(in.input, tableMean, params.tableMeanFrameAlphaInt / 10000.f);
	accumulateWeighted(in.tableDiffNoLow2, tableEstimatedVariance, params.tableVarianceDiff2AlphaInt / 10000.f);

	float ballColorValue = params.ballColorValueInt / 100.f;
	in.ballDiff = in.input - Scalar(ballColorValue, ballColorValue, ballColorValue);
	in.ballDiff2 = in.ballDiff.mul(in.ballDiff);

	float ballColorValueVariance = params.ballColorValueVarianceInt / 10000.f;

	in.ballDiffNorm = in.ballDiff2 / Mat(size, type, Scalar(ballColorValueVariance, ballColorValueVariance, ballColorValueVariance));

	log(tableEstimatedVariance, in.logTableEstimatedVariance);

	transform(in.tableDiffNorm + in.logTableEstimatedVariance, in.tableProb, -Matx<float, 1, 3>(1, 1, 1));

	threshold(-in.tableProb, in.notTableProbTrunc, params.tableProbThresholdInt / 100.f, 0, CV_THRESH_TRUNC);
	in.tableProbTrunc = -in.notTableProbTrunc;

	transform(in.ballDiffNorm, in.ballProb, -Matx<float, 1, 3>(1, 1, 1));
	in.ballProb -= 3 * log(ballColorValueVariance);

	in.pixelProb = in.ballProb - in.tableProbTrunc;
	blur(in.pixelProb, in.posProb, Size(3, 3));

	tableEstimatedVariance.copyTo(in.tableEstimatedVariance);
	tableMean.copyTo(in.tableMean);

//	double maxProb;
//	minMaxLoc(in.posProb, nullptr, &maxProb);

	density.density = in.posProb;

	return density;
}

BallDensityEstimatorInternals BallDensityEstimator::getLastInternals() {
	return lastInternals;
}
