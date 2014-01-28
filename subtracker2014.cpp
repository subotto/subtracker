#include <iostream>
#include <iterator>
#include <string>
#include <deque>

#include <cmath>
#include <chrono>

#include "utility.hpp"
#include "blobs_finder.hpp"
#include "blobs_tracker.hpp"
#include "subotto_metrics.hpp"
#include "subotto_tracking.hpp"
#include "control.hpp"

using namespace cv;
using namespace std;
using namespace chrono;

SubottoReference reference;
SubottoMetrics metrics;

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
	float barx[BARS];
	float distance[BARS];
};

FoosmenMetrics foosmenMetrics;

Trackbar<int> localMaximaLimit("track", "localMaximaLimit", 10, 0, 1000);
Trackbar<float> fps("track", "fps", 120, 0, 2000, 1);
Trackbar<float> localMaximaMinDistance("track", "localMaximaMinDistance", 0.05, 0, 200);

Trackbar<float> foosmenProbThresh("foosmen", "thresh", 2.f, 0.f, 1000.f, 0.1f);

Trackbar<float> goalkeeperx("foosmen", "goalkeeperx", -0.550f, -2.f, 2.f, 0.001f);
Trackbar<float> bar2x("foosmen", "bar2x", -0.395f, -2.f, 2.f, 0.001f);
Trackbar<float> bar5x("foosmen", "bar5x", -0.081f, -2.f, 2.f, 0.001f);
Trackbar<float> bar3x("foosmen", "bar3x", +0.233f, -2.f, 2.f, 0.001f);

Trackbar<float> goalkeeperdistance("foosmen", "goalkeeperdistance", 0.210f, 0.f, 2.f, 0.001f);
Trackbar<float> bar2distance("foosmen", "bar2distance", 0.244f, 0.f, 2.f, 0.001f);
Trackbar<float> bar5distance("foosmen", "bar5distance", 0.118f, 0.f, 2.f, 0.001f);
Trackbar<float> bar3distance("foosmen", "bar3distance", 0.210f, 0.f, 2.f, 0.001f);

Trackbar<float> convWidth("foosmen", "convWidth", 0.025, 0, 0.1, 0.001);
Trackbar<float> convLength("foosmen", "convLength", 0.060, 0, 0.2, 0.001);

Trackbar<float> windowLength("foosmen", "windowLength", 0.100, 0, 0.2, 0.001);
Trackbar<float> goalkeeperWindowLength("foosmen", "goalkeeperWindowLength", 0.010, 0, 0.2, 0.001);

Trackbar<float> rotFactorTrackbar("foosmen", "rotFactor", 40, 0, 100, 0.1);

ColorPicker blueColor("color1", Scalar(0.65f, 0.10f, 0.05f));
ColorPicker redColor("color2", Scalar(0.05f, 0.25f, 0.50f));

ColorQuadraticForm blueColorPrecision("color1Precision", Matx<float, 1, 6>(
		12.f, 3.f, 12.f, -10.f, 10.f, -10.f
));

ColorQuadraticForm redColorPrecision("color2Precision", Matx<float, 1, 6>(
		8.f, 3.f, 8.f, 0.f, +10.f, -25.f
));

table_tracking_params_t table_tracking_params;

Trackbar<float> reference_width_trackbar("reference", "width", &reference.metrics.frameSize.width, 0.f, 5.f, 0.01f);
Trackbar<float> reference_height_trackbar("reference", "height", &reference.metrics.frameSize.height, 0.f, 5.f, 0.01f);

Trackbar<float> reference_offset_x_trackbar("reference", "x", &reference.metrics.offset.x, -0.1f, +0.1f, 0.001f);
Trackbar<float> reference_offset_y_trackbar("reference", "y", &reference.metrics.offset.y, -0.1f, +0.1f, 0.001f);

control_panel_t panel;

BlobsTracker blobs_tracker(panel);

Trackbar<double> _fps("ball tracking", "_fps", &blobs_tracker._fps, 0, 100, 1);
Trackbar<double> _max_speed("ball tracking", "_max_speed", &blobs_tracker._max_speed, 0., +100., 0.1);
Trackbar<double> _max_unseen_distance("ball tracking", "_max_unseen_distance", &blobs_tracker._max_unseen_distance, 0., 1., 0.01);
Trackbar<double> _max_interpolation_time("ball tracking", "_max_interpolation_time", &blobs_tracker._max_interpolation_time, 0., 10., 0.01);

Trackbar<double> _skip_parameter("ball tracking", "_skip_parameter", &blobs_tracker._skip_parameter, -100., +100., 0.1);
Trackbar<double> _variance_parameter("ball tracking", "_variance_parameter", &blobs_tracker._variance_parameter, 0., 10., 0.01);
Trackbar<double> _absent_parameter("ball tracking", "_absent_parameter", &blobs_tracker._absent_parameter, -100., +100., 0.1);
Trackbar<double> _appearance_parameter("ball tracking", "_appearance_parameter", &blobs_tracker._appearance_parameter, -1000., +1000., 0.1);
Trackbar<double> _disappearance_parameter("ball tracking", "_disappearance_parameter", &blobs_tracker._disappearance_parameter, -1000., +1000., 0.1);

float barx(int side, int bar, Size size, SubottoMetrics subottoMetrics, FoosmenMetrics foosmenMetrics) {
	float flip = side ? +1 : -1;
	float x = foosmenMetrics.barx[bar] * flip;
	float xx = (0.5f + x / subottoMetrics.length) * size.width;

	return xx;
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

	return Point2f(m.x, m.y);
}

void computeScatterDiag(const Mat& in, Mat& out) {
	multiply(in, in, out);
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

struct TableDescription {
	Mat mean;
	Mat variance;
	Mat correctedVariance;
};

struct TableAnalysis {
	Mat diff;
	Mat filteredDiff; // filtered difference
	Mat filteredScatter; // filteredDiff * filteredDiff'
	Mat nll; // negated log-likelihood
};

int tableDiffLowFilterStdDev = 40;

void getTableFrame(Mat frame, Mat& tableFrame, Size size, Mat transform) {
	Mat frame32f;
	frame.convertTo(frame32f, CV_32F, 1 / 255.f);

	Mat warpTransform = transform * sizeToUnits(metrics, size);
	warpPerspective(frame32f, tableFrame, warpTransform, size, CV_WARP_INVERSE_MAP | CV_INTER_LINEAR);
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

void computeCorrectedVariance(TableDescription& table) {
	Mat tableMeanLaplacian;
	Laplacian(table.mean, tableMeanLaplacian, -1, 3);
	Mat tableMeanBorders = tableMeanLaplacian.mul(tableMeanLaplacian);
	addWeighted(table.variance, 1, tableMeanBorders, 0.004, 0.002, table.correctedVariance);
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

struct BallAnalysis {
	Mat diff;
	Mat scatter;
	Mat ll; // log-likelihood
};

struct BallDescription {
	Scalar meanColor = Scalar(0.85f, 0.85f, 0.85f);
	float valueVariance = 0.019f;
};

void startBallAnalysis(Mat tableFrame, BallDescription& ball, BallAnalysis& analysis) {
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

void updateMean(TableDescription& table, Mat tableFrame) {
	accumulateWeighted(tableFrame, table.mean, 0.005f);
}

void updateVariance(TableDescription& table, const TableAnalysis& analysis) {
	Mat scatter;
	computeScatterDiag(analysis.diff, scatter);
	accumulateWeighted(scatter, table.variance, 0.005f);
}

struct FoosmenBarMetrics {
	float m2height;
	float m2width;

	int count;

	float xPixels;

	float yPixels;
	float distancePixels;
	float marginPixels;

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

struct FoosmenRunningAverage {
	float shift[BARS][2];
	float rot[BARS][2];

	FoosmenRunningAverage() {
		for(int side = 0; side < 2; side++) {
			for(int bar = 0; bar < BARS; bar++) {
				shift[bar][side] = 0.f;
				rot[bar][side] = 0.f;
			}
		}
	}
};

void setUpFoosmenMetrics() {
	foosmenMetrics.barx[GOALKEEPER] = goalkeeperx.get();
	foosmenMetrics.barx[BAR2] = bar2x.get();
	foosmenMetrics.barx[BAR5] = bar5x.get();
	foosmenMetrics.barx[BAR3] = bar3x.get();

	foosmenMetrics.distance[GOALKEEPER] = goalkeeperdistance.get();
	foosmenMetrics.distance[BAR2] = bar2distance.get();
	foosmenMetrics.distance[BAR5] = bar5distance.get();
	foosmenMetrics.distance[BAR3] = bar3distance.get();
}

void computeFoosmenBarMetrics(SubottoMetrics subottoMetrics, FoosmenMetrics foosmenMetrics, int side, int bar, Size size, FoosmenBarMetrics& barMetrics) {
	barMetrics.side = side;
	barMetrics.bar = bar;

	barMetrics.m2height = size.height / subottoMetrics.width;
	barMetrics.m2width = size.width / subottoMetrics.length;

	barMetrics.xPixels = barx(side, bar, size, subottoMetrics, foosmenMetrics);
	barMetrics.yPixels = size.height / 2;

	barMetrics.count = foosmenMetrics.count[bar];
	barMetrics.distancePixels = barMetrics.m2height * foosmenMetrics.distance[bar];
	barMetrics.marginPixels = size.height - (barMetrics.count - 1) * barMetrics.distancePixels;

	int l = bar == GOALKEEPER ? -barMetrics.m2width * goalkeeperWindowLength.get() : -barMetrics.m2width * windowLength.get();
	int r = barMetrics.m2width * windowLength.get();

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

void computeLL(FoosmenBarMetrics barMetrics, FoosmenBarAnalysis &analysis) {
	Scalar meanColor[] = {blueColor.get(), redColor.get()};
	Matx<float, 1, 6> colorPrecision[] = {blueColorPrecision.getScatterTransform(), redColorPrecision.getScatterTransform()};

	analysis.diff = analysis.tableSlice - meanColor[barMetrics.side];
	computeScatter(analysis.diff, analysis.scatter);

	Mat distance;
	transform(analysis.scatter, distance, colorPrecision[barMetrics.side]);

	Mat distanceTresh;
	threshold(distance, distanceTresh, foosmenProbThresh.get(), 0, THRESH_TRUNC);

	// TODO: subtract properly scaled analysis.tableNLLSlice
	blur(distanceTresh, analysis.nll, Size(barMetrics.m2height * convLength.get(), barMetrics.m2width * convWidth.get()));
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

void findFoosmen(FoosmenBarMetrics barMetrics, FoosmenBarAnalysis &analysis) {
	Point2f m;
	m = subpixelMinimum(analysis.overlapped);

	float rotFactor = rotFactorTrackbar.get();

	analysis.shift = (m.y - barMetrics.marginPixels / 2.f) / barMetrics.m2height;

	float rotSin = (barMetrics.colRange.start + m.x - barMetrics.xPixels) * 2.f / barMetrics.m2width * rotFactor;
	rotSin = max(-1.f, min(1.f, rotSin));
	analysis.rot = asin(rotSin);

//	stringstream ss;
//	ss << barMetrics.side << "-bar-" << barMetrics.bar;
//	show(ss.str(), analysis.overlapped, 200);
}

vector<pair<Point2f, float>> findLocalMaxima(Mat density, int radiusX, int radiusY) {
	typedef pair<Point, float> pi; // point, integer
	typedef pair<Point2f, float> pf; // point, floating point

	Mat dilatedDensity;
	dilate(density, dilatedDensity, Mat::ones(2 * radiusY + 1, 2 * radiusX + 1, CV_8U));

	Mat localMaxMask = (density >= dilatedDensity);

	Mat_<Point> nonZero;
	findNonZero(localMaxMask, nonZero);

	vector<pi> localMaxima;
	for(int i = 0; i < nonZero.rows; i++) {
		Point p = *nonZero[i];
		float w = density.at<float>(p);

		localMaxima.push_back(make_pair(p, w));
	}

	int count = min(localMaxima.size(), size_t(localMaximaLimit.get()));
	nth_element(localMaxima.begin(), localMaxima.begin() + count, localMaxima.end(), [](pi a, pi b) {
		return a.second > b.second;
	});
	localMaxima.resize(count);

	vector<pf> results;
	results.reserve(count);
	for(pi lm : localMaxima) {
		Point p = lm.first;

		// trova la posizione in modo più preciso
		Point2f correction = subpixelMinimum(-density(Range(p.y, p.y+1), Range(p.x, p.x+1)));

		results.push_back(make_pair(Point2f(p.x, p.y) + correction, lm.second));
	}

	return results;
}

void doIt(FrameReader& frameReader) {
	Mat trajReprAvg;

	bool play = true;
	bool debug = false;

	int timeline_span = 120;
	int processed_frames = 60;	// number of frames to be processed for each call to ProcessFrame

	int current_time = 0;
	int initial_time = 0;

	namedWindow( "Display", WINDOW_NORMAL );
	deque<Mat> frames;	// used for display only

	Size tableFrameSize(128, 64);

	TableDescription table;
	BallDescription ball;

	table.mean = Mat(tableFrameSize, CV_32FC3, 0.f);
	table.variance = Mat(tableFrameSize, CV_32FC3, 0.f);

	TableAnalysis tableAnalysis;
	BallAnalysis ballAnalysis;

	FoosmenBarMetrics barsMetrics[BARS][2];
	FoosmenBarAnalysis barsAnalysis[BARS][2];

	Mat density;

	float barsShift[BARS][2];
	float barsRot[BARS][2];

	for(int side = 0; side < 2; side++) {
		for(int bar = 0; bar < BARS; bar++) {
			barsShift[bar][side] = 0.f;
			barsRot[bar][side] = 0.f;
		}
	}
	
	deque< vector<float> > foosmenValues;	// Values to be printed for each frame
	deque<double> timestamps;				// Timestamp of each frame

	thread playback_thread;

	table_tracking_status_t table_tracking_status;

	table_tracking_params.detection.reference = &reference;
	table_tracking_params.following_params.reference = &reference;

	init_table_tracking(table_tracking_status, table_tracking_params, panel);

	for (int i = 0; ; i++) {
		if (true) {
			int c = waitKey(1);

			switch(c) {
			case 'f':
				toggle(panel, "frame", SHOW);
				break;
			case 'c':
				toggle(panel, "cycle", TIME);
				break;
			case 's':
				set_log_level(panel, "table detect", VERBOSE);
				toggle(panel, "table detect", TRACKBAR, true);
				break;
			case 'd':
				debug = !debug;
				break;
			case ' ':
				set_log_level(panel, "table detect", WARNING);
				toggle(panel, "table detect", TRACKBAR, false);
				toggle(panel, "frame", SHOW, false);
				toggle(panel, "cycle", TIME, false);
				break;
			}

			dump_time(panel, "cycle", "wait key");
		}

		dump_time(panel, "cycle", "start cycle");

		auto frame_info = frameReader.get();
		Mat frame = frame_info.data;

		Mat table_transform = track_table(frame_info.data, table_tracking_status, table_tracking_params, panel);

		dump_time(panel, "cycle", "detect subotto");

		show(panel, "frame", "frame", frame);

		if ( current_time >= timeline_span ) {
			timestamps.push_back(duration_cast<duration<double>>(frame_info.playback_time.time_since_epoch()).count());
		}
		
		Mat tableFrame;
		getTableFrame(frame, tableFrame, tableFrameSize, table_transform);
		Size size = tableFrame.size();

		dump_time(panel, "cycle", "warp table frame");

		startTableAnalysis(tableFrame, table, tableAnalysis);
		computeFilteredDiff(tableAnalysis);
		computeScatter(tableAnalysis);
		computeNLL(table, tableAnalysis);

		dump_time(panel, "cycle", "table analysis");

		startBallAnalysis(tableFrame, ball, ballAnalysis);
		computeScatter(ballAnalysis);
		computeLL(ball, ballAnalysis);

		computeDensity(tableAnalysis, ballAnalysis, density);

		dump_time(panel, "cycle", "ball analysis");

		updateMean(table, tableFrame);
		updateVariance(table, tableAnalysis);

		if((i%5) == 0) {
			computeCorrectedVariance(table);
		}

		dump_time(panel, "cycle", "update table description");

		setUpFoosmenMetrics();

		for(int side = 0; side < 2; side++) {
			for(int bar = 0; bar < BARS; bar++) {
				FoosmenBarMetrics& barMetrics = barsMetrics[bar][side];
				FoosmenBarAnalysis& analysis = barsAnalysis[bar][side];

				computeFoosmenBarMetrics(metrics, foosmenMetrics, side, bar, size, barMetrics);

				startFoosmenBarAnalysis(barMetrics, analysis, tableFrame, tableAnalysis);
				computeLL(barMetrics, analysis);
				computeOverlapped(barMetrics, analysis);
				findFoosmen(barMetrics, analysis);

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
					foosmenValuesFrame.push_back( barsShift[bar][side] );
					foosmenValuesFrame.push_back( barsRot[bar][side] );
				}
			}
			foosmenValues.push_back( foosmenValuesFrame );
		}

		if(will_show(panel, "foosmen", "foosmen")) {
			Mat tableFoosmen;
			tableFrame.copyTo(tableFoosmen);
			drawFoosmen(tableFoosmen, metrics, foosmenMetrics, barsShift, barsRot);
			show(panel, "foosmen", "foosmen", tableFoosmen);
		}

		int radiusX = localMaximaMinDistance.get() / metrics.length * tableFrameSize.width;
		int radiusY = localMaximaMinDistance.get() / metrics.width * tableFrameSize.height;
		auto localMaxima = findLocalMaxima(density, radiusX, radiusY);

		// Cambio le unità di misura secondo le costanti in SubottoMetrics
		SubottoMetrics metrics;
		for (int i=0; i<localMaxima.size(); i++) {
			Point2f &p = localMaxima[i].first;

			p.x = (p.x / density.cols - 0.5f) * metrics.length;
			p.y = (p.y / density.rows - 0.5f) * metrics.width;
		}
		
		// Inserisco i punti migliori come nuovo frame nella timeline
		vector<Blob> blobs;
		for (int i=0; i<localMaxima.size(); i++) {
			blobs.push_back( Blob(localMaxima[i].first, 0.0, 0.0, localMaxima[i].second) );
		}

		logger(panel, "ball tracking", VERBOSE) << "Inserting frame " << current_time << " in timeline" << endl;

		blobs_tracker.InsertFrameInTimeline(blobs, current_time);
		
		// Metto da parte il frame per l'eventuale visualizzazione
		if ( current_time >= timeline_span ) {
			frames.push_back(tableFrame);
		}

		if ( current_time >= 2*timeline_span ) {
			blobs_tracker.PopFrameFromTimeline();
			int processed_time = initial_time + timeline_span;

			// Il processing vero e proprio avviene solo ogni k fotogrammi (k=processed_frames)
			if ( initial_time % processed_frames == 0 ) {
				vector<Point2f> positions = blobs_tracker.ProcessFrames( initial_time, processed_time, processed_time + processed_frames );

				for (int i=0; i<positions.size(); i++) {
					int frame_id = processed_time + i;
					
					assert(!timestamps.empty());
					double timestamp = timestamps.front();
					timestamps.pop_front();
					
					assert(!foosmenValues.empty());
					vector<float> foosmenValuesFrame = foosmenValues.front();
					foosmenValues.pop_front();

					printf("%lf", timestamp);
					
					// Controllo che la pallina sia stata effettivamente trovata (la posizione non deve essere NISBA) e la stampo
					if ( positions[i].x < 900.0 && positions[i].y < 900.0 ) printf(",%lf,%lf", positions[i].x, positions[i].y);
					else printf(",,");
					
					for (int j=0; j<foosmenValuesFrame.size(); j++) {
						printf(",%f", foosmenValuesFrame[j]);
					}
					printf("\n");
					fflush(stdout);
				}

				if (debug) {
					// Mostro il primo fotogramma tra quelli processati

					static atomic<bool> stop(false);

					if(playback_thread.joinable()) {
						stop = true;
						playback_thread.join();
					}

					stop = false;
					playback_thread = thread([=] {
						assert(!frames.empty());
						Mat display;
						time_point<system_clock> playback_start = system_clock::now();

						for(int i = 0; i < processed_frames; i++) {
							frames[i].copyTo(display);
							Point2f ball = positions[i];
							fprintf(stderr,"Ball found: (%lf,%lf)\n", ball.x, ball.y);

							ball.x = (ball.x / metrics.length + 0.5f) * density.cols;
							ball.y = (ball.y / metrics.width + 0.5f) * density.rows;

							circle( display, ball, 8, Scalar(0,255,0), 2 );
							imshow("Display", display);

							this_thread::sleep_until(playback_start + i * duration<double>(1. / 20.));

							if(stop.exchange(false)) {
								return;
							}
						}

						stop.exchange(false);
					});
				}

				for (int i=0; i<processed_frames; i++) {
					frames.pop_front();
				}
			}

			initial_time++;
		}

		current_time++;
	}
}

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
		cerr << "<video> can be: " << endl;
		cerr << "\tn (a single digit number) - live capture from video device n" << endl;
		cerr << "\tfilename+ (with a trailing plus) - simulate live capture from video file" << endl;
		cerr << "\tfilename (without a trailing plus) - batch analysis of video file" << endl;
		cerr << "<reference subotto> is the reference image used to look for the table" << endl;
		cerr << "<reference sumotto mask> is an optional B/W image, of the same size," << endl;
		cerr << "\twhere black spots indicate areas of the reference image to hide" << endl;
		cerr << "\twhen looking for the table (such as moving parts and spurious features)" << endl;
		return 1;
	}

	reference.image = imread(referenceImageName);

	if(!referenceImageMaskName.empty()) {
		reference.mask = imread(referenceImageMaskName, CV_LOAD_IMAGE_GRAYSCALE);
	}

	init_control_panel(panel);

	if(videoName.size() == 1) {
		FrameReader f(videoName[0] - '0', panel);
		doIt(f);
	} else {
		bool simulate_live = false;

		if(videoName.back() == '+') {
			videoName = videoName.substr(0, videoName.size()-1);
			simulate_live = true;
		}

		FrameReader f(videoName.c_str(), panel, simulate_live);
		doIt(f);
	}

	return 0;
}
