#include <iostream>
#include <iterator>
#include <string>
#include <utility>

#include <cmath>
#include <limits>
#include <tuple>

#include "utility.hpp"
#include "ball_density.hpp"

using namespace cv;
using namespace std;

VideoCapture cap;

unique_ptr<SubottoTracker> subottoTracker;
SubottoReference reference;
SubottoMetrics metrics;

Size size;

enum {
	GOALKEEPER,
	BAR2,
	BAR5,
	BAR3,
	BARS
};

const int foosmenPerBar[] {
	3,
	2,
	5,
	3
};

struct FoosmenMetrics {
	float barx[BARS];
	float distance[BARS];
};

FoosmenMetrics foosmenMetrics;

void onChange(int a, void* b) {
	subottoTracker = unique_ptr<SubottoTracker>(new SubottoTracker(cap, reference, metrics, SubottoTrackingParams()));
}

Mat getInput(SubottoTracking subotto) {
	Mat frame;
	subotto.frame.convertTo(frame, CV_32F, 1 / 255.f);
	Mat warpTransform = subotto.transform * sizeToUnits(metrics, size);
	Mat input;
	warpPerspective(frame, input, warpTransform, size, CV_WARP_INVERSE_MAP | CV_INTER_LINEAR);
	return input;
}

float barx(int side, int bar, Size size, SubottoMetrics subottoMetrics, FoosmenMetrics foosmenMetrics) {
	float flip = side ? +1 : -1;
	float x = foosmenMetrics.barx[bar] * flip;
	float xx = (0.5f + x / subottoMetrics.length) * size.width;

	return xx;
}

static float zeros[BARS][2];

void drawFoosmen(Mat out, SubottoMetrics subottoMetrics, FoosmenMetrics foosmenMetrics, float shift[][2] = nullptr, float rot[][2] = nullptr) {
	if(!shift) {
		shift = zeros;
	}

	for(int side = 0; side < 2; side++) {
		for(int bar = 0; bar < BARS; bar++) {
			float xx = barx(side, bar, out.size(), subottoMetrics, foosmenMetrics);
			line(out, Point(xx, 0), Point(xx, out.rows), Scalar(1.f, 1.f, 1.f));

			for(int i = 0; i < foosmenPerBar[bar]; i++) {
				float y = (0.5f + i - foosmenPerBar[bar] / 2.f) * foosmenMetrics.distance[bar];
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

		if(d2 > 0)
			correction[k] = -d / d2;
	}

	Point2f c = Point2f(correction[1], correction[0]);

	return Point2f(m.x, m.y) + c;
}

void testFoosmenTracking() {
	bool play = false;

	Trackbar<float> thresh("foosmen", "thresh", 2.f, 0.f, 1000.f, 0.1f);

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

	Trackbar<int> xres("foosmen", "xres", 256, 0, 2000, 1);

	ColorPicker color1("color1", Scalar(0.65f, 0.10f, 0.05f));
	ColorPicker color2("color2", Scalar(0.05f, 0.25f, 0.50f));

	ColorQuadraticForm color1Precision("color1Precision", Matx<float, 1, 6>(
			12.f, 3.f, 12.f, -10.f, 10.f, -10.f
	));

	ColorQuadraticForm color2Precision("color2Precision", Matx<float, 1, 6>(
			8.f, 3.f, 8.f, 0.f, +10.f, -25.f
	));

	float shift[BARS][2];
	float rot[BARS][2];

	while (true) {
		int c = waitKey(play);

		if (c == ' ') {
			play = !play;
		}

		size = Size(xres.get(), xres.get()/2);

		foosmenMetrics.barx[GOALKEEPER] = goalkeeperx.get();
		foosmenMetrics.barx[BAR2] = bar2x.get();
		foosmenMetrics.barx[BAR5] = bar5x.get();
		foosmenMetrics.barx[BAR3] = bar3x.get();

		foosmenMetrics.distance[GOALKEEPER] = goalkeeperdistance.get();
		foosmenMetrics.distance[BAR2] = bar2distance.get();
		foosmenMetrics.distance[BAR5] = bar5distance.get();
		foosmenMetrics.distance[BAR3] = bar3distance.get();

		int frameBlockSize = 50;

		SubottoTracking subotto = subottoTracker->next();

		Mat input = getInput(subotto);

		Scalar meanColor[] = {color1.get(), color2.get()};
		Matx<float, 1, 6> colorPrecision[] = {color1Precision.getScatterTransform(), color2Precision.getScatterTransform()};

		for(int side = 0; side < 2; side++) {
			for(int bar = 0; bar < BARS; bar++) {
				float m2px = size.height / metrics.width;

				float xf = barx(side, bar, size, metrics, foosmenMetrics);
				int x = xf;

				float distancePixels = m2px * foosmenMetrics.distance[bar];
				int b = foosmenPerBar[bar];

				int l = bar == GOALKEEPER ? -m2px * goalkeeperWindowLength.get() : -m2px * windowLength.get();
				int r = m2px * windowLength.get();

				if(!side) {
					l = -l;
					r = -r;
				}

				int marginPixels = size.height - (b-1) * distancePixels;

				Mat slicex = input(Range::all(), Range(x+min(l,r), x+max(l,r)));

				Mat diff = slicex - meanColor[side];

				vector<Mat> diffBGR;
				split(diff, diffBGR);

				Mat diffScatter;
				vector<Mat> diffScatterSplit;

				for(int i = 0; i < 3; i++) {
					diffScatterSplit.push_back(diffBGR[i].mul(diffBGR[i]));
				}

				for(int i = 0; i < 3; i++) {
					diffScatterSplit.push_back(diffBGR[i].mul(diffBGR[(i+1)%3]));
				}
				merge(diffScatterSplit, diffScatter);

				Mat prob;
				transform(diffScatter, prob, colorPrecision[side]);

				Mat probThresh;
				threshold(prob, probThresh, thresh.get(), 0, THRESH_TRUNC);

				Mat s(marginPixels, abs(r-l), CV_32F, 0.f);

				Mat probBlurred;
				blur(probThresh, probBlurred, Size(m2px * convLength.get(), m2px * convWidth.get()));

				for(int i = 0; i < b; i++) {
					float y = (0.5f + i - foosmenPerBar[bar] * 0.5f) * distancePixels;
					int yy = size.height / 2 + y - marginPixels / 2;

					Mat slice = probBlurred(Range(yy, int(yy + marginPixels)), Range::all());
					s += slice;
				}

				Point2f m;
				m = subpixelMinimum(s);

				float cShift = m.y - marginPixels/2.f;
				float shiftAlpha = 0.5f;
				shift[bar][side] = (1 - shiftAlpha) * shift[bar][side] + shiftAlpha * cShift;

				float cRot = (x + min(l,r) + m.x - xf) * 5.f;
				float rotAlpha = 0.2f;
				rot[bar][side] = (1 - rotAlpha) * rot[bar][side] + rotAlpha * cRot;

				stringstream ss;
				ss << side << "-bar-" << bar;
				show(ss.str(), s, 200);
			}

//			show(side ? "diffRed" : "diffBlue", diff, 500, 50);
//			show(side ? "red" : "blue", -probBlurred, 1000, 100);
//			show(side ? "redT" : "blueT", -probThresh, 400, 50);
		}

		Mat fm;
		input.copyTo(fm);
		drawFoosmen(fm, metrics, foosmenMetrics, shift, rot);
		show("fm", fm);

		show("input", input);
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

	onChange(0, 0);

	testFoosmenTracking();

	return 0;
}
