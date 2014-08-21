#ifndef ANALYSIS_HPP_
#define ANALYSIS_HPP_

#include <deque>

#include "opencv2/imgproc/imgproc.hpp"

using namespace std;
using namespace cv;

struct TableDescription {
	Mat mean;
	Mat variance;
	Mat correctedVariance;

  TableDescription(Size tableFrameSize);
};

struct TableAnalysis {
	Mat diff;
	Mat filteredDiff; // filtered difference
	Mat filteredScatter; // filteredDiff * filteredDiff'
	Mat nll; // negated log-likelihood
};


void do_table_analysis(control_panel_t &panel,
                       const Mat &tableFrame,
                       const TableDescription &table,
                       TableAnalysis &tableAnalysis);


struct BallDescription {
	Scalar meanColor = Scalar(0.85f, 0.85f, 0.85f);
	float valueVariance = 0.019f;
};

struct BallAnalysis {
	Mat diff;
	Mat scatter;
	Mat ll; // log-likelihood
};


void do_ball_analysis(control_panel_t &panel,
                      const Mat &tableFrame,
                      const BallDescription& ball,
                      const TableAnalysis& tableAnalysis,
                      BallAnalysis& ballAnalysis,
                      Mat& density);


void do_update_table_description(control_panel_t &panel,
                                 const Mat &tableFrame,
                                 const TableAnalysis& tableAnalysis,
                                 TableDescription& table);

void do_update_corrected_variance(control_panel_t &panel,
                                  TableDescription &table);


enum {
	GOALKEEPER,
	BAR2,
	BAR5,
	BAR3,
	BARS
};

struct FoosmenMetrics {
	int count[4] = {
		3,
		2,
		5,
		3
	};
	float barx[BARS] {
		-0.550f,
		-0.395f,
		-0.081f,
		+0.233f
	};
	float distance[BARS] {
		0.210f,
		0.244f,
		0.118f,
		0.210f
	};
};


struct FoosmenBarMetrics {
  // Scaling ration from subotto coordinates to tableFrame coordinates
	float m2height;
	float m2width;

  // Number of foosmen in the bar
	int count;

  // Base point of the bar (halfway through its length), in tableFrame coordinates
	float xPixels;
	float yPixels;

  // Distance between foosmen in tableFrame coordinates
	float distancePixels;

  // Totale distance aside of the foosmen in tableFrame coordinates
	float marginPixels;

  // Range of x-columns pertaining to the bar, in tableFrame coordinates
	Range colRange;

	int side;
	int bar;
};

struct FoosmenBarAnalysis {
	Mat tableSlice;
	Mat tableNLLSlice;

	Mat diff;
	Mat scatter;
	Mat nll;

	Mat overlapped;

	float shift;
	float rot;
};

struct foosmen_params_t {
	Scalar mean_color[2] {{0.65f, 0.10f, 0.05f}, {0.05f, 0.25f, 0.50f}};
	Matx<float, 1, 6> color_precision[2] {
		{12.f, 3.f, 12.f, -10.f, 10.f, -10.f},
		{8.f, 3.f, 8.f, 0.f, +10.f, -25.f}
	};

	float convolution_width = 0.025;
	float convolution_length = 0.060;

	float window_length = 0.100f;
	float goalkeeper_window_length = 0.010f;

	float rot_factor = 40.f;

	float nll_threshold = 2.f;
};

Point2f subpixelMinimum(control_panel_t &panel, Mat in);
float barx(int side, int bar, Size size, SubottoMetrics subottoMetrics, FoosmenMetrics foosmenMetrics);
void do_foosmen_analysis(control_panel_t &panel,
                         FoosmenBarMetrics barsMetrics[BARS][2],
                         FoosmenBarAnalysis barsAnalysis[BARS][2],
                         const SubottoMetrics &metrics,
                         const FoosmenMetrics &foosmenMetrics,
                         const Size &size,
                         const foosmen_params_t& foosmen_params,
                         const Mat &tableFrame,
                         const TableAnalysis& tableAnalysis,
                         float barsShift[BARS][2],
                         float barsRot[BARS][2]);

#endif /* ANALYSIS_HPP_ */
