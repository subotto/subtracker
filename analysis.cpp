//#include <iostream>
//#include <iterator>
//#include <string>
//#include <deque>

//#include <cmath>
//#include <chrono>

//#include "utility.hpp"
//#include "blobs_finder.hpp"
//#include "blobs_tracker.hpp"
#include "subotto_metrics.hpp"
//#include "subotto_tracking.hpp"
#include "control.hpp"
#include "analysis.hpp"

using namespace cv;
using namespace std;
//using namespace chrono;

int tableDiffLowFilterStdDev = 40;

void computeScatterDiag(const Mat& in, Mat& out) {
	multiply(in, in, out);
}

void fastLargeGaussianBlur(Mat in, Mat& out, float stdDev) {
	Mat temp;
	in.copyTo(temp);
	int boxSize = tableDiffLowFilterStdDev * 3 * sqrt(2 * CV_PI) / 4 + 0.5;
	for(int i = 0; i < 3; i++) {
		blur(temp, out, Size(boxSize, boxSize));
		out.copyTo(temp);
	}
}

void startTableAnalysis(Mat tableFrame, const TableDescription& table, TableAnalysis& analysis) {
	subtract(tableFrame, table.mean, analysis.diff);
}

void computeFilteredDiff(TableAnalysis& analysis) {
	Mat low;
	fastLargeGaussianBlur(analysis.diff, low, tableDiffLowFilterStdDev);
	subtract(analysis.diff, low, analysis.filteredDiff);
}

void computeScatter(TableAnalysis& analysis) {
	computeScatterDiag(analysis.filteredDiff, analysis.filteredScatter);
}

void computeNLL(const TableDescription& table, TableAnalysis& analysis) {
	Mat tableDiffNorm = analysis.filteredScatter / table.correctedVariance;

	Mat logVariance;
	log(table.variance, logVariance);

	Mat notTableProb;
	transform(tableDiffNorm + logVariance, notTableProb, Matx<float, 1, 3>(1, 1, 1));

	Mat notTableProbTrunc;
	float tableProbThresh = 20.f;
	threshold(notTableProb, analysis.nll, tableProbThresh, 0, CV_THRESH_TRUNC);
}


void do_table_analysis(control_panel_t &panel,
                       const Mat &tableFrame,
                       const TableDescription &table,
                       TableAnalysis &tableAnalysis) {

		startTableAnalysis(tableFrame, table, tableAnalysis);
		computeFilteredDiff(tableAnalysis);
		computeScatter(tableAnalysis);
		computeNLL(table, tableAnalysis);

		dump_time(panel, "cycle", "table analysis");

}


void startBallAnalysis(Mat tableFrame, const BallDescription& ball, BallAnalysis& analysis) {
	analysis.diff = tableFrame - ball.meanColor;
}

void computeScatter(BallAnalysis& analysis) {
	computeScatterDiag(analysis.diff, analysis.scatter);
}

void computeLL(const BallDescription& ball, BallAnalysis& analysis) {
	Mat ballDiffNorm = analysis.scatter / ball.valueVariance;

	transform(ballDiffNorm, analysis.ll, -Matx<float, 1, 3>(1, 1, 1));
	analysis.ll -= 3 * log(ball.valueVariance);
}

void computeDensity(const TableAnalysis& tableAnalysis, const BallAnalysis& ballAnalysis, Mat& density) {
	Mat pixelProb = ballAnalysis.ll + tableAnalysis.nll;
	blur(pixelProb, density, Size(3, 3));
}


void do_ball_analysis(control_panel_t &panel,
                      const Mat &tableFrame,
                      const BallDescription& ball,
                      const TableAnalysis& tableAnalysis,
                      BallAnalysis& ballAnalysis,
                      Mat& density) {

		startBallAnalysis(tableFrame, ball, ballAnalysis);
		computeScatter(ballAnalysis);
		computeLL(ball, ballAnalysis);

		computeDensity(tableAnalysis, ballAnalysis, density);

		dump_time(panel, "cycle", "ball analysis");

}



void updateMean(TableDescription& table, Mat tableFrame) {
	accumulateWeighted(tableFrame, table.mean, 0.005f);
}

void updateVariance(TableDescription& table, const TableAnalysis& analysis) {
	Mat scatter;
	computeScatterDiag(analysis.diff, scatter);
	accumulateWeighted(scatter, table.variance, 0.005f);
}

void computeCorrectedVariance(TableDescription& table) {
	Mat tableMeanLaplacian;
	Laplacian(table.mean, tableMeanLaplacian, -1, 3);
	Mat tableMeanBorders = tableMeanLaplacian.mul(tableMeanLaplacian);
	addWeighted(table.variance, 1, tableMeanBorders, 0.004, 0.002, table.correctedVariance);
}


void do_update_table_description(control_panel_t &panel,
                                 const Mat &tableFrame,
                                 const TableAnalysis& tableAnalysis,
                                 const int &i,
                                 TableDescription& table) {

		updateMean(table, tableFrame);
		updateVariance(table, tableAnalysis);

		if((i%5) == 0) {
			computeCorrectedVariance(table);
		}

		dump_time(panel, "cycle", "update table description");

}


float barx(int side, int bar, Size size, SubottoMetrics subottoMetrics, FoosmenMetrics foosmenMetrics) {
	float flip = side ? +1 : -1;
	float x = foosmenMetrics.barx[bar] * flip;
	float xx = (0.5f + x / subottoMetrics.length) * size.width;

	return xx;
}

void computeFoosmenBarMetrics(SubottoMetrics subottoMetrics, FoosmenMetrics foosmenMetrics, int side, int bar, Size size, FoosmenBarMetrics& barMetrics, const foosmen_params_t& params) {
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

void startFoosmenBarAnalysis(FoosmenBarMetrics barMetrics, FoosmenBarAnalysis &analysis, Mat tableFrame, const TableAnalysis& tableAnalysis) {
	Size size = tableFrame.size();
	analysis.tableSlice = tableFrame(Range::all(), barMetrics.colRange);
	analysis.tableNLLSlice = tableAnalysis.nll(Range::all(), barMetrics.colRange);
}

void computeScatter(const Mat& in, Mat& out) {
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

void computeLL(FoosmenBarMetrics barMetrics, FoosmenBarAnalysis &analysis, const foosmen_params_t& params) {
	analysis.diff = analysis.tableSlice - params.mean_color[barMetrics.side];
	computeScatter(analysis.diff, analysis.scatter);

	Mat distance;
	transform(analysis.scatter, distance, params.color_precision[barMetrics.side]);

	Mat distanceTresh;
	threshold(distance, distanceTresh, params.nll_threshold, 0, THRESH_TRUNC);

	// TODO: subtract properly scaled analysis.tableNLLSlice
	blur(distanceTresh, analysis.nll, Size(barMetrics.m2height * params.convolution_length, barMetrics.m2width * params.convolution_width));
}

void computeOverlapped(FoosmenBarMetrics barMetrics, FoosmenBarAnalysis &analysis) {
	analysis.overlapped.create(barMetrics.marginPixels, barMetrics.colRange.size(), CV_32F);
	analysis.overlapped.setTo(0.f);

	for(int i = 0; i < barMetrics.count; i++) {
		float shift = 0.5f + i - barMetrics.count * 0.5f;

		float y = shift * barMetrics.distancePixels;
		int yy = barMetrics.yPixels + y - barMetrics.marginPixels / 2;

		Mat slice = analysis.nll(Range(yy, int(yy + barMetrics.marginPixels)), Range::all());
		analysis.overlapped += slice;
	}
}

Point2f subpixelMinimum(Mat in) {
	Point m;
	minMaxLoc(in, nullptr, nullptr, &m, nullptr);

	Mat p = in(Range(m.y, m.y+1), Range(m.x, m.x+1));

	Mat der[2];
	Mat der2[2];

	Sobel(p, der[0], -1, 0, 1, 1);
	Sobel(p, der[1], -1, 1, 0, 1);
	Sobel(p, der2[0], -1, 0, 2, 1);
	Sobel(p, der2[1], -1, 2, 0, 1);

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

	return Point2f(m.x + c.x, m.y + c.y);
}

void findFoosmen(FoosmenBarMetrics barMetrics, FoosmenBarAnalysis &analysis, const foosmen_params_t& params) {
	Point2f m;
	m = subpixelMinimum(analysis.overlapped);

	analysis.shift = (m.y - barMetrics.marginPixels / 2.f) / barMetrics.m2height;

	float rotSin = (barMetrics.colRange.start + m.x - barMetrics.xPixels) * 2.f / barMetrics.m2width * params.rot_factor;
	rotSin = max(-1.f, min(1.f, rotSin));
	analysis.rot = asin(rotSin);

//	stringstream ss;
//	ss << barMetrics.side << "-bar-" << barMetrics.bar;
//	show(ss.str(), analysis.overlapped, 200);
}

void drawFoosmen(Mat out, SubottoMetrics subottoMetrics, FoosmenMetrics foosmenMetrics, float shift[][2] = nullptr, float rot[][2] = nullptr) {
	static float zeros[BARS][2];

	if(!shift) {
		shift = zeros;
	}

	for(int side = 0; side < 2; side++) {
		for(int bar = 0; bar < BARS; bar++) {
			float xx = barx(side, bar, out.size(), subottoMetrics, foosmenMetrics);
			line(out, Point(xx, 0), Point(xx, out.rows), Scalar(1.f, 1.f, 1.f));

			for(int i = 0; i < foosmenMetrics.count[bar]; i++) {
				float y = (0.5f + i - foosmenMetrics.count[bar] * 0.5f) * foosmenMetrics.distance[bar];
				float yy = (0.5f + (y + shift[bar][side]) / subottoMetrics.width) * out.rows;

				line(out, Point(xx - 5, yy), Point(xx + 5, yy), Scalar(0.f, 1.f, 0.f));

				if(rot) {
					float len = 20;
					float r = rot[bar][side];
					line(out, Point(xx, yy), Point(xx + sin(r) * len, yy + cos(r) * len), Scalar(1.f, 1.f, 0.f), 1, 16);
				}
			}
		}
	}
}


// TODO - Check types: why can't barsMetrics and barsAnalysis have the "&"?
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
                         float barsRot[BARS][2],
                         const int current_time,
                         const int timeline_span,
                         deque< vector<float> > &foosmenValues) {

		for(int side = 0; side < 2; side++) {
			for(int bar = 0; bar < BARS; bar++) {
				FoosmenBarMetrics& barMetrics = barsMetrics[bar][side];
				FoosmenBarAnalysis& analysis = barsAnalysis[bar][side];

				computeFoosmenBarMetrics(metrics, foosmenMetrics, side, bar, size, barMetrics, foosmen_params);

				startFoosmenBarAnalysis(barMetrics, analysis, tableFrame, tableAnalysis);
				computeLL(barMetrics, analysis, foosmen_params);
				computeOverlapped(barMetrics, analysis);
				findFoosmen(barMetrics, analysis, foosmen_params);

				float& shift = barsShift[bar][side];
				float& rot = barsRot[bar][side];

				float shiftAlpha = 0.5f;
				shift = (1-shiftAlpha) * shift + shiftAlpha * analysis.shift;

				float rotAlpha = 0.2f;
				rot = (1-rotAlpha) * rot + rotAlpha * analysis.rot;
			}
		}

		dump_time(panel, "cycle", "foosmen analysis");

		// Saving values for later...
		if ( current_time >= timeline_span ) {
			vector<float> foosmenValuesFrame;
			for(int side = 1; side >= 0; side--) {
				for(int bar = 0; bar < BARS; bar++) {
					foosmenValuesFrame.push_back( -barsShift[bar][side] );
					foosmenValuesFrame.push_back( barsRot[bar][side] );
				}
			}
			foosmenValues.push_back( foosmenValuesFrame );
		}

		if(will_show(panel, "foosmen tracking", "foosmen")) {
			Mat tableFoosmen;
			tableFrame.copyTo(tableFoosmen);
			drawFoosmen(tableFoosmen, metrics, foosmenMetrics, barsShift, barsRot);
			show(panel, "foosmen tracking", "foosmen", tableFoosmen);
		}

}
