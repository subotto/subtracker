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

BallDensityEstimatorParams params;

void onChange(int a, void* b) {
	unique_ptr<SubottoTracker> subottoTracker(new SubottoTracker(cap, reference, SubottoTrackingParams()));
	densityEstimator = unique_ptr<BallDensityEstimator>(new BallDensityEstimator(move(subottoTracker), params));
}

int expShift = 500;

struct StateDensityComponent {
	float weight;

	Vec<float, 2> position;
	Vec<float, 2> velocity;

	float positionVariance;
	float velocityVariance;
	float positionVelocityCovariance;

	bool bounced = false;

	shared_ptr<StateDensityComponent> previous;
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

ostream& operator<< (ostream& ots, StateDensityComponent const& a) {
	ots << "StateDensityComponent[" << endl;
	ots << "weight: " << a.weight << endl;
	ots << "position: " << a.position << endl;
	ots << "velocity: " << a.velocity << endl;
	ots << "positionVariance: " << a.positionVariance << endl;
	ots << "velocityVariance: " << a.velocityVariance << endl;
	ots << "positionVelocityCovariance: " << a.positionVelocityCovariance << endl;
	ots << "]";
	return ots;
}

ostream& operator<< (ostream& ots, BallDensityLocalMaximum const& a) {
	return ots << "BallDensityLocalMaximum[" << a.weight << "," << a.position << "]";
}

void testBallTracking() {
	vector<StateDensityComponent> previousStates;

	namedWindow("trackSlides", CV_WINDOW_NORMAL);

	Trackbar<int> localMaximaLimit("track", "localMaximaLimit", 200, 0, 1000);
	Trackbar<int> statesLimit("track", "statesLimit", 50, 0, 1000);

	Trackbar<float> spawnPenalty("track", "spawnPenalty", 100, 0, 100, 0.01);
	Trackbar<float> localMaximumStdDev("track", "localMaximumStdDev", 1.5, 0.01, 100, 0.01);
	Trackbar<float> positionModelStdDev("track", "positionModelStdDev", 0, 0.001, 10, 0.001);
	Trackbar<float> velocityModelStdDev("track", "velocityModelStdDev", 0.01, 0.001, 10, 0.001);
	Trackbar<float> spawnVelocityStdDev("track", "spawnVelocityStdDev", 0.1, 0.001, 10, 0.001);
	Trackbar<float> bouncePenalty("track", "bouncePenalty", 4, 0, 100, 0.01);

	Trackbar<float> localMaximaMinDistance("track", "localMaximaMinDistance", 5, 0, 200);
	Trackbar<float> statesMinDistance("track", "statesMinDistance", 5, 0, 200, 0.01);

	Trackbar<float> statesMinRelativeVelocity("track", "statesMinRelativeVelocity", 2, 0, 200, 0.01);
	Trackbar<float> statesMinVelocityVarianceDifference("track", "statesMinVelocityVarianceDifference", 5, 0, 200, 0.01);
	Trackbar<float> statesMinPositionVarianceDifference("track", "statesMinPositionVarianceDifference", 5, 0, 200, 0.01);

	Mat trajReprAvg;

	bool play = false;
	while (true) {
		if(waitKey(play) == ' ') {
			play = !play;
		}

		float localMaximumVar = pow(localMaximumStdDev.get(), 2);
		float positionModelVar = pow(positionModelStdDev.get(), 2);
		float velocityModelVar = pow(velocityModelStdDev.get(), 2);

		auto ballDensity = densityEstimator->next();
		auto density = ballDensity.density;
		auto internals = densityEstimator->getLastInternals();

		Mat dilatedDensity;
		dilate(density, dilatedDensity, Mat(), Point(-1, -1), localMaximaMinDistance.get());

		Mat localMaxMask = (density >= dilatedDensity);

		Mat_<Point> nonZero;
		findNonZero(localMaxMask, nonZero);

		vector<BallDensityLocalMaximum> localMaxima;
		for(int i = 0; i < nonZero.rows; i++) {
			Point p = *nonZero[i];
			float weight = density.at<float>(p);

			localMaxima.push_back(BallDensityLocalMaximum(weight, Vec<float, 2>(p.x, p.y)));
		}

		sort(localMaxima.begin(), localMaxima.end(), [](BallDensityLocalMaximum a, BallDensityLocalMaximum b) {
			return a.weight > b.weight;
		});
		localMaxima.resize(min(localMaxima.size(), size_t(localMaximaLimit.get())));

		float maxWeight = -1e6;
		for(auto s : previousStates) {
			maxWeight = max(maxWeight, s.weight);
		}

		for(auto& s : previousStates) {
			s.weight -= maxWeight;
		}

		StateDensityComponent spawn;

		spawn.position = Vec<float, 2>(0, 0);
		spawn.velocity = Vec<float, 2>(0, 0);

		spawn.positionVariance = 1e6;
		spawn.velocityVariance = pow(spawnVelocityStdDev.get(), 2);
		spawn.positionVelocityCovariance = 0;

		spawn.weight = -spawnPenalty.get();

		previousStates.push_back(spawn);

		vector<StateDensityComponent> currentStates;
		for(StateDensityComponent ps : previousStates) {
			Vec<float, 2> predictedPos = ps.position + ps.velocity;
			Vec<float, 2> predictedVel = ps.velocity;

			float predictedPosVar = ps.positionVariance + ps.velocityVariance + 2*ps.positionVelocityCovariance + positionModelVar;
			float predictedVelVar = ps.velocityVariance + velocityModelVar;
			float predictedPosVelCovar = ps.positionVelocityCovariance + ps.velocityVariance;

			StateDensityComponent ups;

			ups.position = predictedPos;
			ups.velocity = predictedVel;

			ups.positionVariance = predictedPosVar;
			ups.velocityVariance = predictedVelVar;
			ups.positionVelocityCovariance = predictedPosVelCovar;
			ups.weight = ps.weight;

			ups.previous = shared_ptr<StateDensityComponent>(new StateDensityComponent(ps));

			currentStates.push_back(ups);

			for(BallDensityLocalMaximum lm : localMaxima) {
				StateDensityComponent cs;

				Vec<float, 2> residual = lm.position - predictedPos;
				float residualVar = predictedPosVar + localMaximumVar;

				float cost = sum(residual.mul(residual))[0] / residualVar;

				float positionGain = predictedPosVar / residualVar;
				float velocityGain = predictedPosVelCovar / residualVar;

				Vec<float, 2> correctedPos = predictedPos + positionGain * residual;
				Vec<float, 2> correctedVel = predictedVel + velocityGain * residual;

				float correctedPosVar = predictedPosVar * (1 - positionGain);
				float correctedVelVar = predictedVelVar - velocityGain * predictedPosVelCovar;
				float correctedPosVelCovar = predictedPosVelCovar * (1 - positionGain);

				if(correctedPosVar <= 0 ||
						correctedVelVar <= 0 ||
						correctedPosVelCovar <= 0 ||
						correctedPosVelCovar >= correctedVelVar + correctedPosVar) {
					cout << "Previous: " << ps << endl;
					cout << "Local Maximum: " << lm << endl;

					cout << "predictedPos: " << predictedPos << endl;
					cout << "predictedVel: " << predictedVel << endl;

					cout << "predictedPosVar: " << predictedPosVar << endl;
					cout << "predictedVelVar: " << predictedVelVar << endl;
					cout << "predictedPosVelCovar: " << predictedPosVelCovar << endl;

					cout << "residual: " << residual << endl;
					cout << "residualVar: " << residualVar << endl;

					cout << "positionGain: " << positionGain << endl;
					cout << "velocityGain: " << velocityGain << endl;

					cout << "correctedPos: " << correctedPos << endl;
					cout << "correctedVel: " << correctedVel << endl;

					cout << "correctedPosVar: " << correctedPosVar << endl;
					cout << "correctedVelVar: " << correctedVelVar << endl;
					cout << "correctedPosVelCovar: " << correctedPosVelCovar << endl;

					cout << "Current: " << cs << endl;

					throw logic_error("negative variance");
				}

				cs.position = correctedPos;
				cs.velocity = correctedVel;

				cs.positionVariance = correctedPosVar;
				cs.velocityVariance = correctedVelVar;
				cs.positionVelocityCovariance = correctedPosVelCovar;

				cs.weight = ps.weight + lm.weight - cost;

				cs.previous = shared_ptr<StateDensityComponent>(new StateDensityComponent(ps));

				currentStates.push_back(cs);

				StateDensityComponent bounce = cs;

				bounce.velocity = 0;
				bounce.velocityVariance = pow(spawnVelocityStdDev.get(), 2);
				bounce.weight -= bouncePenalty.get();
				bounce.bounced = true;

				currentStates.push_back(bounce);
			}
		}

		sort(currentStates.begin(), currentStates.end(), [](StateDensityComponent a, StateDensityComponent b) {
			return a.weight > b.weight;
		});

		previousStates.clear();
		for(auto s : currentStates) {
			if(previousStates.size() >= statesLimit.get()) {
				break;
			}

			if(!Rect_<int>(Point(0, 0), density.size()).contains(Point(s.position[0], s.position[1]))) {
				continue;
			}


			bool dropped = false;
			for(auto ps : previousStates) {
				if(
						norm(s.position - ps.position) < statesMinDistance.get() &&
						norm(s.velocity - ps.velocity) < statesMinRelativeVelocity.get() &&
						abs(s.velocityVariance - ps.velocityVariance) < statesMinVelocityVarianceDifference.get() &&
						abs(s.positionVariance - ps.positionVariance) < statesMinPositionVarianceDifference.get()) {
					dropped = true;
					break;
				}
			}

			if(dropped) {
				continue;
			}

			previousStates.push_back(s);
		}

		maxWeight = -1e6;
		for(auto s : previousStates) {
			maxWeight = max(maxWeight, s.weight);
		}

		if(true) {
			Mat stateRepr;
			internals.input.copyTo(stateRepr);

			vector<StateDensityComponent> reversedStates = previousStates;
			reverse(previousStates.begin(), previousStates.end());

			for(auto s : previousStates) {
				Vec<float, 2> pv = s.position + s.velocity * 10;
				Point p(s.position[0], s.position[1]);
				Point pp(pv[0], pv[1]);

				float intensity = 1.f - (maxWeight - s.weight) / 30;

				circle(stateRepr, pp, max(1.f, sqrt(s.velocityVariance) * 3), s.bounced ? Scalar(intensity, intensity, 0) : Scalar(intensity, 0, 0));
				line(stateRepr, p, pp, Scalar(intensity, 0, 0));
				circle(stateRepr, p, max(1.f, sqrt(s.positionVariance)), Scalar(0, 0, intensity));
			}
			show("stateRepr", stateRepr);
		}

		StateDensityComponent best = previousStates.back();

		StateDensityComponent* current = &best;
		Mat trajRepr = Mat::zeros(internals.input.size(), CV_32FC3);
		for(int i = 0; i < 100; i++) {
			if(current == nullptr) {
				break;
			}

			StateDensityComponent* previous = current->previous.get();

			if(previous == nullptr) {
				break;
			}

			Point p(current->position[0], current->position[1]);
			Point pp(previous->position[0], previous->position[1]);

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
		show("trajReprAvg", internals.input + trajReprAvg);

		Mat localMaxMaskF;
		localMaxMask.convertTo(localMaxMaskF, CV_32F, 1/255.f);

		show("frame", ballDensity.tracking.frame);
		show("input", internals.input);
		show("localMaxMask", localMaxMask);
		show("density", density);
		show("dilatedDensity", dilatedDensity);
		show("selected", localMaxMaskF.mul(density));
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
