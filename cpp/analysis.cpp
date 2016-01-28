#include "subotto_metrics.hpp"
#include "control.hpp"
#include "analysis.hpp"

using namespace cv;
using namespace std;

int tableDiffLowFilterStdDev = 40;

TableDescription::TableDescription(Size tableFrameSize) {

  this->mean = Mat(tableFrameSize, CV_32FC3, 0.f);
  this->variance = Mat(tableFrameSize, CV_32FC3, 0.f);

}

void TableDescription::set_first_frame(Mat firstFrame) {

     firstFrame.copyTo(this->mean);
     // At the beginning we know nearly nothing about the actual table
     this->variance = Mat(this->mean.size(), CV_32FC3, 1000.f);

}

void do_table_analysis(control_panel_t &panel,
                       const Mat &tableFrame,
                       const TableDescription &table,
                       TableAnalysis &tableAnalysis) {

	subtract(tableFrame, table.mean, tableAnalysis.diff);

	Mat low;
	Mat temp;
	tableAnalysis.diff.copyTo(temp);
	int boxSize = tableDiffLowFilterStdDev * 3 * sqrt(2 * CV_PI) / 4 + 0.5;
	for(int i = 0; i < 3; i++) {
		blur(temp, low, Size(boxSize, boxSize));
		low.copyTo(temp);
	}
	subtract(tableAnalysis.diff, low, tableAnalysis.filteredDiff);

	multiply(tableAnalysis.filteredDiff, tableAnalysis.filteredDiff, tableAnalysis.filteredScatter);

	Mat tableDiffNorm = tableAnalysis.filteredScatter / table.correctedVariance;

	Mat logVariance;
	log(table.variance, logVariance);

	Mat notTableProb;
	transform(tableDiffNorm + logVariance, notTableProb, Matx<float, 1, 3>(1, 1, 1));

	//Mat notTableProbTrunc;
	float tableProbThresh = 20.f;
	threshold(notTableProb, tableAnalysis.nll, tableProbThresh, 0, CV_THRESH_TRUNC);
  notTableProb.copyTo(tableAnalysis.nll);

  dump_time(panel, "cycle", "table analysis");

}


void do_ball_analysis(control_panel_t &panel,
                      const Mat &tableFrame,
                      const BallDescription& ball,
                      const TableAnalysis& tableAnalysis,
                      BallAnalysis& ballAnalysis,
                      Mat& density) {

	ballAnalysis.diff = tableFrame - ball.meanColor;

	multiply(ballAnalysis.diff, ballAnalysis.diff, ballAnalysis.scatter);

	Mat ballDiffNorm = ballAnalysis.scatter / ball.valueVariance;

	transform(ballDiffNorm, ballAnalysis.ll, -Matx<float, 1, 3>(1, 1, 1));
	ballAnalysis.ll -= log(ball.valueVariance);

	Mat pixelProb = 0.5 * ballAnalysis.ll + tableAnalysis.nll;
	blur(pixelProb, density, Size(3, 3));

  dump_time(panel, "cycle", "ball analysis");

}


void do_update_table_description(control_panel_t &panel,
                                 const Mat &tableFrame,
                                 const TableAnalysis& tableAnalysis,
                                 TableDescription& table) {

	accumulateWeighted(tableFrame, table.mean, 0.005f);

	Mat scatter;
	multiply(tableAnalysis.diff, tableAnalysis.diff, scatter);
	accumulateWeighted(scatter, table.variance, 0.005f);

  dump_time(panel, "cycle", "update table description");

}

void do_update_corrected_variance(control_panel_t &panel,
                                  TableDescription &table) {

	Mat tableMeanLaplacian;
	Laplacian(table.mean, tableMeanLaplacian, -1, 3);
	Mat tableMeanBorders = tableMeanLaplacian.mul(tableMeanLaplacian);
	addWeighted(table.variance, 1, tableMeanBorders, 0.004, 0.002, table.correctedVariance);

  dump_time(panel, "cycle", "update corrected variance");

}


// x coordinate of a certain bar in the tableFrame reference
float barx(int side, int bar, Size size, SubottoMetrics subottoMetrics, FoosmenMetrics foosmenMetrics) {
	float flip = side ? +1 : -1;
	float x = foosmenMetrics.barx[bar] * flip;
	float xx = (0.5f + x / subottoMetrics.length) * size.width;

	return xx;
}

static void computeFoosmenBarMetrics(SubottoMetrics subottoMetrics, FoosmenMetrics foosmenMetrics, int side, int bar, Size size, FoosmenBarMetrics& barMetrics, const foosmen_params_t& params) {
	barMetrics.side = side;
	barMetrics.bar = bar;

	barMetrics.m2height = size.height / subottoMetrics.width;
	barMetrics.m2width = size.width / subottoMetrics.length;

	barMetrics.xPixels = barx(side, bar, size, subottoMetrics, foosmenMetrics);
	barMetrics.yPixels = size.height / 2;

	barMetrics.count = foosmenMetrics.count[bar];
	barMetrics.distancePixels = barMetrics.m2height * foosmenMetrics.distance[bar];
	barMetrics.marginPixels = size.height - (barMetrics.count - 1) * barMetrics.distancePixels;

	int l = bar == GOALKEEPER ? -barMetrics.m2width * params.goalkeeper_window_length : -barMetrics.m2width * params.window_length;
	int r = barMetrics.m2width * params.window_length;
	if(!side) {
		l = -l;
		r = -r;
	}
	barMetrics.colRange = Range(barMetrics.xPixels + min(l,r), barMetrics.xPixels + max(l,r));
}

static void startFoosmenBarAnalysis(FoosmenBarMetrics barMetrics, FoosmenBarAnalysis &analysis, Mat tableFrame, const TableAnalysis& tableAnalysis) {
	analysis.tableSlice = tableFrame(Range::all(), barMetrics.colRange);
	analysis.tableNLLSlice = tableAnalysis.nll(Range::all(), barMetrics.colRange);
}

static void computeScatter(const Mat& in, Mat& out) {
	vector<Mat> inBGR;
	split(in, inBGR);

	vector<Mat> outSplit;

	for(int i = 0; i < 3; i++) {
		outSplit.push_back(inBGR[i].mul(inBGR[i]));
	}

	for(int i = 0; i < 3; i++) {
		outSplit.push_back(inBGR[i].mul(inBGR[(i+1)%3]));
	}

	merge(outSplit, out);
}

static void computeLL(FoosmenBarMetrics barMetrics, FoosmenBarAnalysis &analysis, const foosmen_params_t& params) {
	analysis.diff = analysis.tableSlice - params.mean_color[barMetrics.side];
	computeScatter(analysis.diff, analysis.scatter);

	Mat distance;
	transform(analysis.scatter, distance, params.color_precision[barMetrics.side]);

	Mat distanceTresh;
	threshold(distance, distanceTresh, params.nll_threshold, 0, THRESH_TRUNC);

	// TODO: subtract properly scaled analysis.tableNLLSlice
	blur(distanceTresh, analysis.nll, Size(barMetrics.m2height * params.convolution_length, barMetrics.m2width * params.convolution_width));
}

// Select the marginPixels-sized boxes centered at the foosmen's base
// position and sum them up in the same matrix
static void computeOverlapped(FoosmenBarMetrics barMetrics, FoosmenBarAnalysis &analysis) {
	analysis.overlapped.create(barMetrics.marginPixels, barMetrics.colRange.size(), CV_32F);
	analysis.overlapped.setTo(0.f);

	for(int i = 0; i < barMetrics.count; i++) {
    float shift = -0.5f * (barMetrics.count - 1) + i;
		float y = shift * barMetrics.distancePixels;
		int yy = barMetrics.yPixels + y - barMetrics.marginPixels / 2;
		Mat slice = analysis.nll(Range(yy, int(yy + barMetrics.marginPixels)), Range::all());
		analysis.overlapped += slice;
	}
}

Point2f subpixelMinimum(control_panel_t &panel, Mat in) {
	Point m;
	minMaxLoc(in, nullptr, nullptr, &m, nullptr);

  // This is a one-pixel matrix; how can it extract any derivative
  // from it?
	Mat p = in(Range(m.y, m.y+1), Range(m.x, m.x+1));
  //logger(panel, "gio", DEBUG) << "p: " << p << endl;

	Mat der[2];
	Mat der2[2];

	Sobel(p, der[0], -1, 0, 1, 1);
	Sobel(p, der[1], -1, 1, 0, 1);
	Sobel(p, der2[0], -1, 0, 2, 1);
	Sobel(p, der2[1], -1, 2, 0, 1);
  //logger(panel, "gio", DEBUG) << "derivatives: " << der[0] << der[1] << der2[0] << der2[1] << endl;

	float correction[2] {0.f, 0.f};
	for(int k = 0; k < 2; k++) {
		float d = der[k].at<float>(0, 0);
		float d2 = der2[k].at<float>(0, 0);

		assert(d2 >= 0);

		if(d2 > 0) {
			float ratio = -0.5f * d / d2;

			correction[k] = max(-0.5f, min(ratio, +0.5f));
		}

		assert(abs(correction[k]) <= 1.f);
	}

	Point2f c = Point2f(correction[1], correction[0]);
  //logger(panel, "gio", DEBUG) << "correction: " << c << endl;

	return Point2f(m.x + c.x, m.y + c.y);
}

static void findFoosmen(control_panel_t &panel, FoosmenBarMetrics barMetrics, FoosmenBarAnalysis &analysis, const foosmen_params_t& params) {
	Point2f m;
	m = subpixelMinimum(panel, analysis.overlapped);

	analysis.shift = (m.y - barMetrics.marginPixels / 2.f) / barMetrics.m2height;

	float rotSin = (barMetrics.colRange.start + m.x - barMetrics.xPixels) * 2.f / barMetrics.m2width * params.rot_factor;
	rotSin = max(-1.f, min(1.f, rotSin));
	analysis.rot = asin(rotSin);

//	stringstream ss;
//	ss << barMetrics.side << "-bar-" << barMetrics.bar;
//	show(ss.str(), analysis.overlapped, 200);
}


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
                         float barsRot[BARS][2]) {

		for(int side = 0; side < 2; side++) {
			for(int bar = 0; bar < BARS; bar++) {
				FoosmenBarMetrics& barMetrics = barsMetrics[bar][side];
				FoosmenBarAnalysis& analysis = barsAnalysis[bar][side];

				computeFoosmenBarMetrics(metrics, foosmenMetrics, side, bar, size, barMetrics, foosmen_params);

				startFoosmenBarAnalysis(barMetrics, analysis, tableFrame, tableAnalysis);
				computeLL(barMetrics, analysis, foosmen_params);
				computeOverlapped(barMetrics, analysis);
				findFoosmen(panel, barMetrics, analysis, foosmen_params);

				float& shift = barsShift[bar][side];
				float& rot = barsRot[bar][side];

				float shiftAlpha = 0.5f;
				shift = (1-shiftAlpha) * shift + shiftAlpha * analysis.shift;

				float rotAlpha = 0.2f;
				rot = (1-rotAlpha) * rot + rotAlpha * analysis.rot;
			}
		}

		dump_time(panel, "cycle", "foosmen analysis");

}
