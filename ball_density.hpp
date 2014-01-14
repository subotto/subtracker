#ifndef BALL_DENSITY_HPP_
#define BALL_DENSITY_HPP_

#include "subotto_tracking.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/video/video.hpp>
#include <memory>

struct BallDensityEstimatorParams {
	int tableMeanFrameAlphaInt = 10;

	int tableVarianceDiff2AlphaInt = 10;
	int tableVarianceBordersAlphaInt = 40;
	int tableVarianceGammaInt = 20;

	int tableMeanLaplacianSize = 2;
	int tableDiffLowFilterStdDev = 40;

	int ballColorValueInt = 85;
	int ballColorValueVarianceInt = 190;

	int tableProbThresholdInt = 2000;
};

struct BallDensityEstimatorInternals {
	cv::Mat input;
	cv::Mat tableMean;
	cv::Mat tableDiff;
	cv::Mat tableDiffLow;
	cv::Mat tableDiffNoLow;
	cv::Mat tableDiffNoLow2;
	cv::Mat tableMeanLaplacian;
	cv::Mat tableMeanBorders;
	cv::Mat tableEstimatedVariance;
	cv::Mat tableCorrectedVariance;
	cv::Mat logTableEstimatedVariance;
	cv::Mat tableDiffNorm;
	cv::Mat ballDiff;
	cv::Mat ballDiff2;
	cv::Mat ballDiffNorm;
	cv::Mat tableProb;
	cv::Mat notTableProbTrunc;
	cv::Mat tableProbTrunc;
	cv::Mat ballProb;
	cv::Mat pixelProb;
	cv::Mat posProb;
};

struct BallDensity {
	SubottoTracking tracking;
	cv::Mat density;
};

class BallDensityEstimator {
public:
	BallDensityEstimator(std::unique_ptr<SubottoTracker> subottoTracker, BallDensityEstimatorParams params);
	virtual ~BallDensityEstimator();

	BallDensity next();
	BallDensityEstimatorInternals getLastInternals();
private:
	std::unique_ptr<SubottoTracker> subottoTracker;
	std::unique_ptr<cv::BackgroundSubtractor> backgroundSubtractor;

	SubottoMetrics metrics;
	BallDensityEstimatorParams params;
	BallDensityEstimatorInternals lastInternals;

	cv::Mat tableMean;
	cv::Mat tableEstimatedVariance;

	cv::Mat getInput(const BallDensity& density);
};

#endif /* BALL_DENSITY_HPP_ */
