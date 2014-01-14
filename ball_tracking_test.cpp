#include <iostream>
#include <iterator>
#include <string>
#include <utility>

#include <cmath>

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

struct State {
	float logLikelihood2;

	Vec<float, 2> pos;
	Vec<float, 2> vel;

	float posVar;
	float velVar;
	float posVelCov;

	bool bounced = false;
	float secondsSinceLastBounce = 0;

	bool ballLost = false;

	shared_ptr<State> previous;

	Mat var() {
		return (Mat_<float>(2, 2) << posVar, posVelCov, posVelCov, velVar);
	}
};

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

ostream& operator<< (ostream& ots, State const& a) {
	ots << "StateDensityComponent[" << endl;
	ots << "  weight: " << a.logLikelihood2 << endl;
	ots << "  position: " << a.pos << endl;
	ots << "  velocity: " << a.vel << endl;
	ots << "  positionVariance: " << a.posVar << endl;
	ots << "  velocityVariance: " << a.velVar << endl;
	ots << "  posVelCov: " << a.posVelCov << endl;
	ots << "]";
	return ots;
}

ostream& operator<< (ostream& ots, BallDensityLocalMaximum const& a) {
	return ots << "BallDensityLocalMaximum[" << a.weight << "," << a.position << "]";
}

struct ModelParams {
	float positionVolatility2;
	float velocityVolatility2;
};

struct Observation {
	Vec<float, 2> pos;
	float posVar;
	float weight;
};

State predict(State s, ModelParams params) {
	// TODO: specify time step as parameter

	State t;

	t.pos = s.pos + s.vel;
	t.vel = s.vel;

	t.posVar = s.posVar + s.velVar + 2*s.posVelCov + params.positionVolatility2;
	t.velVar = s.velVar + params.velocityVolatility2;
	t.posVelCov = s.posVelCov + s.velVar;

	t.logLikelihood2 = s.logLikelihood2;

	return t;
}

State correct(State before, Observation o, ModelParams params) {
	State after;

	Vec<float, 2> residual = o.pos - before.pos;

	float residualVar = before.posVar + o.posVar;

	float cost = sum(residual.mul(residual))[0] / residualVar + log(before.posVar);

	float posGain = before.posVar / residualVar;
	float velGain = before.posVelCov / residualVar;

	after.pos = before.pos + posGain * residual;
	after.vel = before.vel + velGain * residual;

	after.posVar = before.posVar * (1 - posGain);
	after.velVar = before.velVar - velGain * before.posVelCov;
	after.posVelCov = before.posVelCov * (1 - posGain);

	after.logLikelihood2 = before.logLikelihood2 - cost + o.weight;

	return after;
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

	show("selected", selectedDilated, 10, 20);

	return localMaxima;
}

float kl(State q, State p) {
	Mat pVarInv;

	invert(p.var(), pVarInv);

//	cout << "p.var(): " << p.var() << endl;
//	cout << "q.var(): " << q.var() << endl;
//	cout << "pVarInv: " << pVarInv << endl;

	float diffNorm = 0;
	for(int c = 0; c < 2; c++) {
		Mat px = (Mat_<float>(1, 2) << p.pos[c], p.vel[c]);
		Mat qx = (Mat_<float>(1, 2) << p.pos[c], p.vel[c]);
		Mat d = px - qx;

		Mat normD = d * pVarInv * d.t();

//		cout << "d[" << c << "]: " << d << endl;

		diffNorm += normD.at<float>(0, 0);
	}

	float tr = trace(pVarInv * q.var())[0];
	float logDetRatio = -log(determinant(q.var()) / determinant(p.var()));
	float kl = tr + 0.5f * diffNorm - 2 + 0.5f * logDetRatio;

//	cout << "tr: " << tr << endl;
//	cout << "diffNorm: " << diffNorm << endl;
//	cout << "logDetRatio: " << logDetRatio << endl;
//	cout << "KL: " << kl << endl;
//	cout << endl;

	return kl;
}

void testBallTracking() {
	ModelParams model;

	vector<State> previousStates;

	Trackbar<int> localMaximaLimit("track", "localMaximaLimit", 5, 0, 1000);
	Trackbar<int> statesLimit("track", "statesLimit", 20, 0, 10000);

	Trackbar<float> fps("track", "fps", 120, 0, 2000, 1);

	Trackbar<float> bouncesPerSecond("track", "bouncesPerSecond", 5, 0, 100, 0.1);
	Trackbar<float> spawnsPerSecond("track", "spawnsPerSecond", 0.1, 0, 1, 0.001);

	Trackbar<float> losesPerSecond("track", "losesPerSecond", 2, 0, 10, 0.1);
	Trackbar<float> loseAvgDuration("track", "loseAvgDuration", 0.05, 0, 1, 0.01);

	Trackbar<float> localMaximumStdDev("track", "localMaximumStdDev", 2, 0.01, 100, 0.01);

	Trackbar<float> positionVolatility("track", "positionVolatility", 0, 0.001, 10, 0.001);
	Trackbar<float> velocityVolatility("track", "velocityVolatility", 0.1, 0.001, 10, 0.001);

	Trackbar<float> spawnVelocityStdDev("track", "spawnVelocityStdDev", 200, 0, 1000, 1);

	Trackbar<float> localMaximaMinDistance("track", "localMaximaMinDistance", 5, 0, 200);

	Trackbar<float> minKL("track", "minKL", 0.1, 0, 10, 0.1);

	Trackbar<float> minBouncesInterval("track", "minBouncesInterval", 0.1, 0, 100, 0.1);

//	Trackbar<float> statesMinDistance("track", "statesMinDistance", 20, 0, 200, 0.01);
//	Trackbar<float> statesMinRelativeVelocity("track", "statesMinRelativeVelocity", 50, 0, 200, 0.01);
//	Trackbar<float> statesMinVelocityVarianceDifference("track", "statesMinVelocityVarianceDifference", 2, 0, 200, 0.01);
//	Trackbar<float> statesMinPositionVarianceDifference("track", "statesMinPositionVarianceDifference", 2, 0, 200, 0.01);

	Mat trajReprAvg;

	bool play = false;
	bool debug = false;
	while (true) {
		int c = waitKey(play);

		if (c == ' ') {
			play = !play;
		}

		if (c == 'd') {
			debug = !debug;
		}

		float localMaximumVar = pow(localMaximumStdDev.get(), 2);

		model.positionVolatility2 = pow(positionVolatility.get(), 2) / fps.get();
		model.velocityVolatility2 = pow(velocityVolatility.get(), 2) / fps.get();

		auto ballDensity = densityEstimator->next();
		auto density = ballDensity.density;
		auto internals = densityEstimator->getLastInternals();

		auto localMaxima = findLocalMaxima(density, localMaximaMinDistance.get());
		sort(localMaxima.begin(), localMaxima.end(), [](BallDensityLocalMaximum a, BallDensityLocalMaximum b) {
			return a.weight > b.weight;
		});
		localMaxima.resize(min(localMaxima.size(), size_t(localMaximaLimit.get())));

		float maxLikelihood = -1e6;
		for(auto s : previousStates) {
			maxLikelihood = max(maxLikelihood, s.logLikelihood2);
		}

		for(auto& s : previousStates) {
			s.logLikelihood2 -= maxLikelihood;
		}

		cout << "maxLikelihood: " << maxLikelihood << endl;

		State spawn;

		spawn.pos = Vec<float, 2>(0, 0);
		spawn.vel = Vec<float, 2>(0, 0);

		spawn.posVar = 1e6;
		spawn.velVar = pow(spawnVelocityStdDev.get() / fps.get(), 2);
		spawn.posVelCov = 0;

		spawn.logLikelihood2 = 2 * log(spawnsPerSecond.get() / fps.get());

		previousStates.push_back(spawn);

		vector<State> currentStates;
		for(State ps : previousStates) {
			State predicted = predict(ps, model);

			State ups = predicted;

			float ballLostProbability;
			if(ps.ballLost) {
				ballLostProbability = 1 - (loseAvgDuration.get() / fps.get());
			} else {
				ballLostProbability = (losesPerSecond.get() / fps.get());
			}

			ups.logLikelihood2 += 2 * log(ballLostProbability);

			ups.ballLost = true;
			ups.secondsSinceLastBounce = ps.secondsSinceLastBounce + 1/fps.get();
			ups.previous = shared_ptr<State>(new State(ps));

			currentStates.push_back(ups);

			for(BallDensityLocalMaximum lm : localMaxima) {
				Observation o;
				o.pos = lm.position;
				o.posVar = localMaximumVar;
				o.weight = lm.weight;

				State cs = correct(predicted, o, model);
				cs.previous = shared_ptr<State>(new State(ps));
				cs.secondsSinceLastBounce = ps.secondsSinceLastBounce + 1/fps.get();

				currentStates.push_back(cs);

				if(ps.secondsSinceLastBounce > minBouncesInterval.get() / fps.get()) {
					State bounce = cs;

					bounce.bounced = true;

					bounce.vel = 0;
					bounce.velVar = pow(spawnVelocityStdDev.get() / fps.get(), 2);

					bounce.logLikelihood2 += 2 * log(bouncesPerSecond.get() / fps.get());
					bounce.secondsSinceLastBounce = 0;

					currentStates.push_back(bounce);
				}
			}
		}

		sort(currentStates.begin(), currentStates.end(), [](State a, State b) {
			return a.logLikelihood2 > b.logLikelihood2;
		});

		previousStates.clear();
		for(auto s : currentStates) {
			if(previousStates.size() >= statesLimit.get()) {
				break;
			}

			bool dropped = false;
			for(auto ps : previousStates) {
				if(kl(ps, s) < minKL.get()) {
					dropped = true;
					break;
				}
			}

			if(dropped) {
				continue;
			}

			previousStates.push_back(s);
		}

		if(true) {
			float maxWeight = -1e6;
			for(auto s : previousStates) {
				maxWeight = max(maxWeight, s.logLikelihood2);
			}

			Mat stateRepr;
			internals.input.copyTo(stateRepr);

			vector<State> reversedStates = previousStates;
			reverse(previousStates.begin(), previousStates.end());

			for(auto s : previousStates) {
				Vec<float, 2> pv = s.pos + s.vel * 5;
				Point p(s.pos[0], s.pos[1]);
				Point pp(pv[0], pv[1]);

				float intensity = 1.f - (maxWeight - s.logLikelihood2) / 30;

				circle(stateRepr, pp, max(1.f, sqrt(s.velVar)), Scalar(intensity, 0, 0));
				line(stateRepr, p, pp, Scalar(intensity, 0, 0));
				circle(stateRepr, p, max(1.f, sqrt(s.posVar)), Scalar(0, 0, intensity));
			}
			show("stateRepr", stateRepr);
		}

		State best = previousStates.back();

		State* current = &best;
		Mat trajRepr = Mat::zeros(internals.input.size(), CV_32FC3);
		for(int i = 0; i < 500; i++) {
			if(current == nullptr) {
				break;
			}

			State* previous = current->previous.get();

			if(previous == nullptr) {
				break;
			}

			Point p(current->pos[0], current->pos[1]);
			Point pp(previous->pos[0], previous->pos[1]);

			if(previous->previous != nullptr) {
				line(trajRepr, p, pp, Scalar(1, 0, 0));

				if(current->bounced) {
					circle(trajRepr, p, 3, Scalar(1, 1, 0));
				}
			}

			current = previous;
		}
		show("trajRepr", trajRepr);

		if(trajReprAvg.empty()) {
			trajRepr.copyTo(trajReprAvg);
		}

		trajReprAvg = 10 * trajRepr;
		show("trajReprAvg", 0.5f * internals.input + trajReprAvg);

		show("frame", ballDensity.tracking.frame);
		show("input", internals.input);
		show("density", density, 5, 50);
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
