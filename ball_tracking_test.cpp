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

unique_ptr<BallDensityEstimator> densityEstimator;
SubottoReference reference;
SubottoMetrics metrics;

BallDensityEstimatorParams params;

void onChange(int a, void* b) {
	unique_ptr<SubottoTracker> subottoTracker(new SubottoTracker(cap, reference, metrics, SubottoTrackingParams()));
	densityEstimator = unique_ptr<BallDensityEstimator>(new BallDensityEstimator(move(subottoTracker), params));
}

struct ModelParams {
	float positionVolatility2;
	float velocityVolatility2;
};

#if 0

float kl(State q, State p) {
	Mat pVarInv;

	invert(p.var(), pVarInv);

	float diffNorm = 0;
	for(int c = 0; c < 2; c++) {
		Mat px = (Mat_<float>(1, 2) << p.pos[c], p.vel[c]);
		Mat qx = (Mat_<float>(1, 2) << p.pos[c], p.vel[c]);
		Mat d = px - qx;

		Mat normD = d * pVarInv * d.t();

		diffNorm += normD.at<float>(0, 0);
	}

	float tr = trace(pVarInv * q.var())[0];
	float logDetRatio = -log(determinant(q.var()) / determinant(p.var()));
	float kl = tr + 0.5f * diffNorm - 2 + 0.5f * logDetRatio;

	return kl;
}

#endif

struct lm_t {
	float d;
	int t;
	Point2f x;
};

const float INF = numeric_limits<float>::infinity();

vector<lm_t> highestLocalMaxima(vector<Mat> const& input, size_t count = 200) {

	int s = input.size();

	vector<Mat> b(s), c(s);
	for(int t = 0; t < s; t++) {
		dilate(input[t], b[t], Mat(), Point(-1, -1), 5);
	}

	for(int t = 0; t < s; t++) {
		auto& l = t > 0 ? input[t-1] : input[t];
		auto& r = t < s-1 ? input[t+1] : input[t];

		c[t] = b[t]; // max(max(b[t], l), r);

		show("i", input[t], 5, 50);
		show("l", l, 5, 50);
		show("r", r, 5, 50);
		show("b", b[t], 5, 50);
		show("c", c[t], 5, 50);
		show("mask", input[t] >= c[t]);
		waitKey();
	}

	vector<lm_t> highest;

	for(int t = 0; t < s; t++) {
		Mat mask = input[t] >= c[t];
		const Mat& y = input[t];

		for(int i = 0; i < mask.rows; i++) {
			uchar* p = mask.ptr(i);
			for(int j = 0; j < mask.cols; j++) {
				if(p[i]) {
					highest.push_back({y.at<float>(i, j), t, Point2f(j, i)});
				}
			}
		}
	}

	size_t n = min(count, highest.size());
	nth_element(highest.begin(), highest.begin() + n, highest.end(), [](const lm_t& a, const lm_t& b) {
		return a.d > b.d;
	});

	highest.resize(n);

	// sort chronologically
	sort(highest.begin(), highest.end(), [](const lm_t& a, const lm_t& b) {
		return a.t < b.t;
	});

	return highest;
}

struct particles_t {
	// position
	Mat x;

	// position variance
	Mat xvar;

	// velocity
	Mat xdot;

	// velocity variance
	Mat xdotvar;

	// position velocity covariance
	Mat covar;

	particles_t()
	{}

	particles_t(int time, int nparticles)
	:
		x(time, nparticles, CV_32FC2, Scalar(0, 0)),
		xvar(time, nparticles, CV_32FC2, INF),
		xdot(time, nparticles, CV_32FC2, Scalar(0, 0)),
		xdotvar(time, nparticles, CV_32FC2, INF),
		covar(time, nparticles, CV_32FC2, 0)
	{
	}

	particles_t at(int time) {
		particles_t r;

		r.x = x.row(time);
		r.xvar = xvar.row(time);
		r.xdot = xdot.row(time);
		r.xdotvar = xdotvar.row(time);
		r.covar = covar.row(time);

		return r;
	}
};

struct measure_t {
	// difference
	Mat dx;

	// variance
	Mat dxvar;

	measure_t(int time, int nparticles)
	:
		dx(time, nparticles, CV_32FC2, 0),
		dxvar(time, nparticles, CV_32FC2, INF)
	{
	}
};

struct symm22_t {
	Mat xvar;
	Mat xdotvar;
	Mat covar;
};

void invert(symm22_t in, symm22_t out) {
	Mat det = in.xvar.mul(in.xdotvar) - in.covar.mul(in.covar);

	out.xvar = in.xdotvar / det;
	out.xdotvar = in.xvar / det;
	out.covar = -in.covar / det;
}

void predict(particles_t in, particles_t out, float dt) {
	float xdotvolatility2 = 0.f;
	float dt2 = dt * dt;

	out.x = in.x + dt * in.xdot;
	out.xdot = in.xdot;

	out.xvar = in.xvar + 2*dt * in.covar + dt2 * in.xdotvar;
	out.xdotvar = in.xdotvar + dt * xdotvolatility2;
	out.covar = in.covar + dt * in.xdotvar;
}

void correct(particles_t in, particles_t out, measure_t correction) {
	Mat dxvar = correction.dxvar + in.xvar;

	Mat xgain = in.xvar / dxvar;
	Mat xdotgain = in.covar / dxvar;

	out.x = in.x + xgain.mul(correction.dx);
	out.xdot = in.xdot + xdotgain.mul(correction.dx);

	out.xvar = in.xvar.mul(1 - xgain);
	out.xdotvar = in.xdotvar - in.covar.mul(xdotgain);
	out.covar = in.covar.mul(1 - xgain);
}

void combine(particles_t a, particles_t b, particles_t out) {
	particles_t ab[] {a, b};

	symm22_t omega[2];

	for(int i = 0; i < 2; i++) {
		symm22_t sigma;
		sigma.xvar = a.xvar;
		sigma.xdotvar = a.xdotvar;
		sigma.covar = a.covar;

		invert(sigma, omega[i]);
	}

	symm22_t omegaab;

	omegaab.xvar = omega[0].xvar + omega[1].xvar;
	omegaab.xdotvar = omega[0].xdotvar + omega[1].xdotvar;
	omegaab.covar = omega[0].covar + omega[1].covar;

	symm22_t sigmaab;

	invert(omegaab, sigmaab);

	out.xvar = sigmaab.xvar;
	out.xdotvar = sigmaab.xdotvar;
	out.covar = sigmaab.covar;

	Mat xx = Mat::zeros(a.x.size(), a.x.type());
	Mat xxdot = Mat::zeros(a.xdot.size(), a.xdot.type());

	for(int i = 0; i < 2; i++) {
		xx += omega[i].xvar.mul(ab[i].x) + omega[i].covar.mul(ab[i].xdot);
		xxdot += omega[i].covar.mul(ab[i].x) + omega[i].xdotvar.mul(ab[i].xdot);
	}

	out.x = out.xvar.mul(xx) + out.covar.mul(xxdot);
	out.xdot = out.covar.mul(xx) + out.xdotvar.mul(xxdot);
}

void showParticles(string name, particles_t p) {
	Mat image(120, 240, CV_8UC3, Scalar(0, 0, 0));

	for(int i = 0; i < p.x.rows; i++) {
		for(int j = 0; j < p.x.cols; j++) {
			Point2f x = p.x.at<Point2f>(i, j);

			circle(image, x, 2, Scalar(255, 255, 255));
		}
	}

	show(name, image);

	waitKey(1);
}

void maximumAP(vector<Mat> density) {
	int time = density.size();
	int nparticles = 100;

	// allocate and initialize

	particles_t particles[2] { particles_t(time, nparticles), particles_t(time, nparticles) };

	// initial particles

	vector<lm_t> lms = highestLocalMaxima(density, nparticles);

	for(lm_t lm : lms) {
		show("density", density[lm.t]);

		Mat b(120, 240, CV_8UC3, 0);

		float d = density[lm.t].at<float>(int(lm.x.y), int(lm.x.x));
		int a = d * 255.f * 0.01f;

		circle(b, Point(lm.x.x, lm.x.y), 0, Scalar(a, a, a));

		show("localMaximum", b);
		waitKey();
	}

	for(int i = 0; i < lms.size(); i++) {
		particles_t& P = particles[0];

		lm_t& lm = lms[i];

		int t = lm.t;

		P.x.col(i).setTo(static_cast<Vec2f>(lm.x));
		P.xvar.at<Point2f>(t, i) = Vec2f(INF, INF);
		P.xdot.at<Point2f>(t, i) = Vec2f(0, 0);
		P.xdotvar.at<Point2f>(t, i) = Vec2f(INF, INF);
	}

	for(int t = 0; t < time; t++) {
		showParticles("particles", particles[0].at(t));
		show("density", density[t]);
		waitKey();
	}

	int iterations = 5;
	for(int iteration = 0; iteration < iterations; iteration++) {
		// evolve particles

		for(int dir = 0; dir < 2; dir++) {
			particles_t& fwd = particles[dir];
			particles_t& bwd = particles[!dir];

			int tstart = dir ? time-1 : 0;
			int tend = dir ? 0 : 1;
			int tstep = dir ? -1 : 1;

			particles_t p(1, nparticles);

			for(int t = tstart; t != tend; t += tstep) {
			}
		}
	}
}

void testBallTracking() {
	ModelParams model;

	Trackbar<float> fps("track", "fps", 120, 0, 2000, 1);

	Trackbar<float> bouncesPerSecond("track", "bouncesPerSecond", 5, 0, 100, 0.1);
	Trackbar<float> spawnsPerSecond("track", "spawnsPerSecond", 0.1, 0, 1, 0.001);

	Trackbar<float> losesPerSecond("track", "losesPerSecond", 2, 0, 10, 0.1);
	Trackbar<float> loseAvgDuration("track", "loseAvgDuration", 0.05, 0, 1, 0.01);

	Trackbar<float> positionVolatility("track", "positionVolatility", 0, 0.001, 10, 0.001);
	Trackbar<float> velocityVolatility("track", "velocityVolatility", 0.1, 0.001, 10, 0.001);

	Trackbar<float> spawnVelocityStdDev("track", "spawnVelocityStdDev", 200, 0, 1000, 1);

	bool play = false;
	while (true) {
		int c = waitKey(play);

		if (c == ' ') {
			play = !play;
		}

		model.positionVolatility2 = pow(positionVolatility.get(), 2) / fps.get();
		model.velocityVolatility2 = pow(velocityVolatility.get(), 2) / fps.get();

		int frameBlockSize = 50;

		vector<Mat> input(frameBlockSize), density(frameBlockSize);

		for (int i = 0; i < frameBlockSize; i++) {
			auto info = densityEstimator->next();
			auto internals = densityEstimator->getLastInternals();

			info.density.copyTo(density[i]);
			internals.input.copyTo(input[i]);
		}

		maximumAP(density);

		for(int i = 0; i < frameBlockSize; i++) {
			int c = waitKey(play);

			if (c == ' ') {
				play = !play;
			}

			show("density", density[i], 5, 50);
//			show("input", frame(thisFrame));
		}
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

	testBallTracking();

	return 0;
}
