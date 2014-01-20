#include <iostream>
#include <iterator>
#include <string>
#include <deque>

#include <cmath>

#include "utility.hpp"
#include "ball_density.hpp"
#include "blobs_finder.hpp"
#include "blobs_tracker.hpp"
#include "subotto_metrics.hpp"

using namespace cv;
using namespace std;

VideoCapture cap;

SubottoReference reference;
SubottoMetrics metrics;
unique_ptr<SubottoTracker> tracker;

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

Trackbar<int> localMaximaLimit("track", "localMaximaLimit", 5, 0, 1000);
Trackbar<float> fps("track", "fps", 120, 0, 2000, 1);
Trackbar<float> localMaximaMinDistance("track", "localMaximaMinDistance", 5, 0, 200);

Trackbar<float> foosmenProbThresh("foosmen", "thresh", 2.f, 0.f, 1000.f, 0.1f);

Trackbar<float> goalkeeperx("foosmen", "goalkeeperx", -0.550f, -2.f, 2.f, 0.001f);
Trackbar<float> bar2x("foosmen", "bar2x", -0.387f, -2.f, 2.f, 0.001f);
Trackbar<float> bar5x("foosmen", "bar5x", -0.076f, -2.f, 2.f, 0.001f);
Trackbar<float> bar3x("foosmen", "bar3x", +0.235f, -2.f, 2.f, 0.001f);

Trackbar<float> goalkeeperdistance("foosmen", "goalkeeperdistance", 0.210f, 0.f, 2.f, 0.001f);
Trackbar<float> bar2distance("foosmen", "bar2distance", 0.244f, 0.f, 2.f, 0.001f);
Trackbar<float> bar5distance("foosmen", "bar5distance", 0.118f, 0.f, 2.f, 0.001f);
Trackbar<float> bar3distance("foosmen", "bar3distance", 0.210f, 0.f, 2.f, 0.001f);

Trackbar<float> convWidth("foosmen", "convWidth", 0.025, 0, 0.1, 0.001);
Trackbar<float> convLength("foosmen", "convLength", 0.060, 0, 0.2, 0.001);

Trackbar<float> windowLength("foosmen", "windowLength", 0.100, 0, 0.2, 0.001);
Trackbar<float> goalkeeperWindowLength("foosmen", "goalkeeperWindowLength", 0.010, 0, 0.2, 0.001);

ColorPicker blueColor("color1", Scalar(0.65f, 0.10f, 0.05f));
ColorPicker redColor("color2", Scalar(0.05f, 0.25f, 0.50f));

ColorQuadraticForm blueColorPrecision("color1Precision", Matx<float, 1, 6>(
		12.f, 3.f, 12.f, -10.f, 10.f, -10.f
));

ColorQuadraticForm redColorPrecision("color2Precision", Matx<float, 1, 6>(
		8.f, 3.f, 8.f, 0.f, +10.f, -25.f
));

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
				float y = (0.5f + i - foosmenMetrics.count[bar] / 2.f) * foosmenMetrics.distance[bar];
				float yy = (0.5f + y / subottoMetrics.width) * out.rows + shift[bar][side];

				line(out, Point(xx - 5, yy), Point(xx + 5, yy), Scalar(0.f, 1.f, 0.f));

				if(rot) {
					circle(out, Point(xx + rot[bar][side], yy), 3, Scalar(1.f, 1.f, 0.f));
				}
			}
		}
	}
}

Point2f subpixelMinimum(Mat in) {
	Point m;
	minMaxLoc(in, nullptr, nullptr, &m, nullptr);

//	Mat p = in(Range(m.y, m.y+1), Range(m.x, m.x+1));
//
//	Mat der[2];
//	Mat der2[2];
//
//	Sobel(p, der[0], -1, 0, 1, 1);
//	Sobel(p, der[1], -1, 1, 0, 1);
//	Sobel(p, der2[0], -1, 0, 2, 1);
//	Sobel(p, der2[1], -1, 2, 0, 1);
//
//	float correction[2] {0.f, 0.f};
//	for(int k = 0; k < 2; k++) {
//		float d = der[k].at<float>(0, 0);
//		float d2 = der2[k].at<float>(0, 0);
//
//		assert(d2 >= 0);
//
//		if(d2 > 0) {
//			float ratio = -d / d2;
//
//			if(abs(ratio) <= 0.5f)
//				correction[k] = ratio;
//		}
//
//		assert(abs(correction[k]) <= 1.f);
//	}
//
//	Point2f c = Point2f(correction[1], correction[0]);

	return Point2f(m.x, m.y);
}

struct BallDensityLocalMaximum {
	float weight;
	Vec<float, 2> position;

	BallDensityLocalMaximum(float weight, Vec<float, 2> position)
		: weight(weight), position(position) {
	}

	BallDensityLocalMaximum()
		: BallDensityLocalMaximum(0, 0)
	{}
};

ostream& operator<< (ostream& ots, BallDensityLocalMaximum const& a) {
	return ots << "BallDensityLocalMaximum[" << a.weight << "," << a.position << "]";
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

struct FoosmenParams {
	float sliceLength;
	float goalkeeperSliceLength;
};

struct FoosmenBarAnalysis {
	float m2px;

	float xPixels;
	float distancePixels;
	float marginPixels;

	Range colRange;

	Mat tableSlice;

	Mat diff;
	Mat scatter;
	Mat nll;

	Mat overlapped;
};

void startFoosmenBarAnalysis(SubottoMetrics subottoMetrics, FoosmenMetrics foosmenMetrics, int side, int bar, FoosmenBarAnalysis &analysis, Mat tableFrame) {
	Size size = tableFrame.size();

	analysis.m2px = size.height / subottoMetrics.width;

	int count = foosmenMetrics.count[bar];

	analysis.xPixels = barx(side, bar, size, subottoMetrics, foosmenMetrics);
	analysis.distancePixels = analysis.m2px * foosmenMetrics.distance[bar];
	analysis.marginPixels = size.height - (count-1) * analysis.distancePixels;

	int l = bar == GOALKEEPER ? -analysis.m2px * goalkeeperWindowLength.get() : -analysis.m2px * windowLength.get();
	int r = analysis.m2px * windowLength.get();

	if(!side) {
		l = -l;
		r = -r;
	}

	analysis.colRange = Range(analysis.xPixels + min(l,r), analysis.xPixels + max(l,r));
	analysis.tableSlice = tableFrame(Range::all(), analysis.colRange);
}

void computeLL(int side, int bar, FoosmenBarAnalysis &analysis) {
	Scalar meanColor[] = {blueColor.get(), redColor.get()};
	Matx<float, 1, 6> colorPrecision[] = {blueColorPrecision.getScatterTransform(), redColorPrecision.getScatterTransform()};

	analysis.diff = analysis.tableSlice - meanColor[side];
	computeScatter(analysis.diff, analysis.scatter);

	Mat distance;
	transform(analysis.scatter, distance, colorPrecision[side]);

	Mat distanceTresh;
	threshold(distance, distanceTresh, foosmenProbThresh.get(), 0, THRESH_TRUNC);

	blur(distanceTresh, analysis.nll, Size(analysis.m2px * convLength.get(), analysis.m2px * convWidth.get()));
}

void computeOverlapped(int side, int bar, FoosmenMetrics foosmenMetrics, Size size, FoosmenBarAnalysis &analysis) {
	analysis.overlapped.create(analysis.marginPixels, analysis.colRange.size(), CV_32F);
	analysis.overlapped.setTo(0.f);

	int count = foosmenMetrics.count[bar];

	for(int i = 0; i < count; i++) {
		float y = (0.5f + i - count * 0.5f) * analysis.distancePixels;
		int yy = size.height / 2 + y - analysis.marginPixels / 2;

		Mat slice = analysis.nll(Range(yy, int(yy + analysis.marginPixels)), Range::all());
		analysis.overlapped += slice;
	}
}

vector<BallDensityLocalMaximum> findLocalMaxima(Mat density, int radius) {
	Mat dilatedDensity;
	dilate(density, dilatedDensity, Mat(), Point(-1, -1), 2 * radius);

	Mat localMaxMask = (density >= dilatedDensity);

	Mat_<Point> nonZero;
	findNonZero(localMaxMask, nonZero);

	vector<BallDensityLocalMaximum> localMaxima;
	for(int i = 0; i < nonZero.rows; i++) {
		Point p = *nonZero[i];
		float weight = density.at<float>(p);

		localMaxima.push_back(BallDensityLocalMaximum(weight, Vec<float, 2>(p.x, p.y)));
	}

	Mat localMaxMaskF;
	localMaxMask.convertTo(localMaxMaskF, CV_32F, 1/255.f);

	Mat selectedDilated;
	dilate(localMaxMaskF.mul(density) - 1e3 * (1 - localMaxMaskF), selectedDilated, Mat(), Point(-1, -1), 1);

//	show("selected", selectedDilated, 10, 20);

	return localMaxima;
}

void doIt() {
	Mat trajReprAvg;

	bool play = true;
	bool debug = false;

	BlobsTracker blobs_tracker;

	int timeline_span = 30;
	int processed_frames = 5;	// number of frames to be processed for each call to ProcessFrame

	int current_time = 0;
	int initial_time = 0;

	namedWindow( "Display", WINDOW_NORMAL );
	deque<Mat> frames;	// used for display only

	tracker = unique_ptr<SubottoTracker>(new SubottoTracker(cap, reference, metrics, SubottoTrackingParams()));

	Size tableFrameSize(240, 120);

	TableDescription table;
	BallDescription ball;

	table.mean = Mat(120, 240, CV_32FC3, 0.f);
	table.variance = Mat(120, 240, CV_32FC3, 0.f);

	TableAnalysis tableAnalysis;
	BallAnalysis ballAnalysis;

	Mat density;

	float shift[BARS][2];
	float rot[BARS][2];

	for(int side = 0; side < 2; side++) {
		for(int bar = 0; bar < BARS; bar++) {
			shift[bar][side] = 0.f;
			rot[bar][side] = 0.f;
		}
	}
	
	deque< vector<float> > foosmenValues;	// Values to be printed for each frame
	deque<double> timestamps;				// Timestamp of each frame

	for (int i = 0; ; i++) {
		int c = waitKey(1);

		if (c == ' ') {
			play = !play;
		}

		if (c == 'd') {
			debug = !debug;
		}

		auto subotto = tracker->next();

		Mat frame = subotto.frame;
		
		// TODO: inserire il vero timestamp!
		timestamps.push_back(1234.0);

		Mat tableFrame;
		getTableFrame(frame, tableFrame, tableFrameSize, subotto.transform);

		Size size = tableFrame.size();
		float m2px = size.height / metrics.width;

		foosmenMetrics.barx[GOALKEEPER] = goalkeeperx.get();
		foosmenMetrics.barx[BAR2] = bar2x.get();
		foosmenMetrics.barx[BAR5] = bar5x.get();
		foosmenMetrics.barx[BAR3] = bar3x.get();

		foosmenMetrics.distance[GOALKEEPER] = goalkeeperdistance.get();
		foosmenMetrics.distance[BAR2] = bar2distance.get();
		foosmenMetrics.distance[BAR5] = bar5distance.get();
		foosmenMetrics.distance[BAR3] = bar3distance.get();

		Scalar meanColor[] = {blueColor.get(), redColor.get()};
		Matx<float, 1, 6> colorPrecision[] = {blueColorPrecision.getScatterTransform(), redColorPrecision.getScatterTransform()};
		
		for(int side = 0; side < 2; side++) {
			for(int bar = 0; bar < BARS; bar++) {
				FoosmenBarAnalysis analysis;

				startFoosmenBarAnalysis(metrics, foosmenMetrics, side, bar, analysis, tableFrame);
				computeLL(side, bar, analysis);
				computeOverlapped(side, bar, foosmenMetrics, size, analysis);

				Point2f m;
				m = subpixelMinimum(analysis.overlapped);

				float cShift = m.y - analysis.marginPixels / 2.f;
				float shiftAlpha = 0.5f;
				shift[bar][side] = (1 - shiftAlpha) * shift[bar][side] + shiftAlpha * cShift;

				float cRot = (analysis.colRange.start + m.x - analysis.xPixels) * 5.f;
				float rotAlpha = 0.2f;
				rot[bar][side] = (1 - rotAlpha) * rot[bar][side] + rotAlpha * cRot;
			}

//			show(side ? "diffRed" : "diffBlue", diff, 500, 50);
//			show(side ? "red" : "blue", -probBlurred, 1000, 100);
//			show(side ? "redT" : "blueT", -probThresh, 400, 50);
		}
		
		
		// Saving values for later...
		vector<float> foosmenValuesFrame;
		for(int side = 0; side < 2; side++) {
			for(int bar = 0; bar < BARS; bar++) {
				foosmenValuesFrame.push_back( shift[bar][side] );
				foosmenValuesFrame.push_back( rot[bar][side] );
			}
		}
		foosmenValues.push_back( foosmenValuesFrame );
		
		Mat fm;
		tableFrame.copyTo(fm);
		drawFoosmen(fm, metrics, foosmenMetrics, shift, rot);
		show("fm", fm);

		startTableAnalysis(tableFrame, table, tableAnalysis);
		computeFilteredDiff(tableAnalysis);
		computeScatter(tableAnalysis);
		computeNLL(table, tableAnalysis);

		startBallAnalysis(tableFrame, ball, ballAnalysis);
		computeScatter(ballAnalysis);
		computeLL(ball, ballAnalysis);

		computeDensity(tableAnalysis, ballAnalysis, density);

		updateMean(table, tableFrame);
		updateVariance(table, tableAnalysis);

		if((i%5) == 0) {
			computeCorrectedVariance(table);
		}

		auto localMaxima = findLocalMaxima(density, localMaximaMinDistance.get());

		nth_element(localMaxima.begin(), localMaxima.begin() + min(localMaxima.size(), size_t(localMaximaLimit.get())), localMaxima.end(), [](BallDensityLocalMaximum a, BallDensityLocalMaximum b) {
			return a.weight > b.weight;
		});
		localMaxima.resize(min(localMaxima.size(), size_t(localMaximaLimit.get())));
		
		
		// Cambio le unità di misura secondo le costanti in SubottoMetrics
		SubottoMetrics metrics;
		for (int i=0; i<localMaxima.size(); i++) {
			// printf("Coordinate prima: (%lf, %lf)\n", localMaxima[i].position[0], localMaxima[i].position[1]);
			localMaxima[i].position[0] *= metrics.length / density.cols;
			localMaxima[i].position[1] *= metrics.width / density.rows;
			// printf("Coordinate dopo:  (%lf, %lf)\n", localMaxima[i].position[0], localMaxima[i].position[1]);
		}
		
		// Inserisco i punti migliori come nuovo frame nella timeline
		vector<Blob> blobs;
		for (int i=0; i<localMaxima.size(); i++) {
			blobs.push_back( Blob(localMaxima[i].position, 0.0, 0.0, localMaxima[i].weight) );
		}
		if (debug) fprintf(stderr, "Inserting frame %d in timeline.\n", current_time);
		blobs_tracker.InsertFrameInTimeline(blobs, current_time);
		
		
		// Metto da parte il frame per l'eventuale visualizzazione
		if ( current_time >= timeline_span ) {
			frames.push_back(tableFrame);
		}


		if ( current_time >= 2*timeline_span ) {
			blobs_tracker.PopFrameFromTimeline();
			int processed_time = initial_time + timeline_span;

			// Il processing vero e proprio avviene solo ogni k fotogrammi (k=processed_frames)
			if ( processed_time % processed_frames == 0 ) {
				vector<Point2f> positions = blobs_tracker.ProcessFrames( initial_time, processed_time, processed_time + processed_frames, debug );

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
					if ( positions[i].x > -0.5 && positions[i].y > -0.5 ) printf(",%lf,%lf", positions[i].x, positions[i].y);
					else printf(",,");
					
					for (int j=0; j<foosmenValuesFrame.size(); j++) {
						printf(",%f", foosmenValuesFrame[j]);
					}
					printf("\n");
					fflush(stdout);
				}

				if (debug) {
					// Mostro il primo fotogramma tra quelli processati

					Mat display;
					assert(!frames.empty());
					frames.front().copyTo(display);
					Point2f ball = positions[0];
					fprintf(stderr,"Ball found: (%lf,%lf)\n", ball.x, ball.y);
					ball.x /= metrics.length / density.cols;
					ball.y /= metrics.width / density.rows;
					circle( display, ball, 2, Scalar(0,255,0), 1 );
					imshow("Display", display);
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
		return 1;
	}

	cap.open(videoName);

	reference.image = imread(referenceImageName);

	if(!referenceImageMaskName.empty()) {
		reference.mask = imread(referenceImageMaskName, CV_LOAD_IMAGE_GRAYSCALE);
	}

	doIt();

	return 0;
}
