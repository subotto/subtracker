#ifndef BALL_DENSITY_HPP_
#define BALL_DENSITY_HPP_

#include "subotto_tracking.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/video/video.hpp>
#include <memory>

struct BallDensity {
	SubottoTracking tracking;
	cv::Mat density;
};

class BallDensityEstimator {
public:
	BallDensityEstimator(std::unique_ptr<SubottoTracker> subottoTracker);
	virtual ~BallDensityEstimator();

	BallDensity next();
private:
	std::unique_ptr<SubottoTracker> subottoTracker;
	std::unique_ptr<cv::BackgroundSubtractor> backgroundSubtractor;

	cv::Mat tableMean;
	cv::Mat tableEstimatedVariance;

	cv::Mat getInput(const BallDensity& density);
};

#endif /* BALL_DENSITY_HPP_ */
