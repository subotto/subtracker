#include <iostream>
#include <iterator>
#include <string>
#include <utility>

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

int localMaximaLimit = 200;
int statesLimit = 50;

int localMaximumStdDevInt = 1500;
int positionModelStdDevInt = 0;
int velocityModelStdDevInt = 1;

int spawnVelocityStdDevInt = 800;

int spawnPenaltyInt = 5000;
int bouncePenaltyInt = 300;

float stdDevIntToVar(int stdDevInt) {
	float stdDev = max(1, stdDevInt) / 1000.f;
	return stdDev * stdDev;
}

void testBallTracking() {
	vector<StateDensityComponent> previousStates;

	namedWindow("trackSlides", CV_WINDOW_NORMAL);

	createTrackbar("localMaximaLimit", "trackSlides", &localMaximaLimit, 200);
	createTrackbar("statesLimit", "trackSlides", &statesLimit, 200);
	createTrackbar("spawnPenaltyInt", "trackSlides", &spawnPenaltyInt, 100000);
	createTrackbar("localMaximumStdDevInt", "trackSlides", &localMaximumStdDevInt, 10000);
	createTrackbar("positionModelStdDevInt", "trackSlides", &positionModelStdDevInt, 10000);
	createTrackbar("velocityModelStdDevInt", "trackSlides", &velocityModelStdDevInt, 10000);
	createTrackbar("spawnVelocityStdDevInt", "trackSlides", &spawnVelocityStdDevInt, 10000);
	createTrackbar("bouncePenaltyInt", "trackSlides", &bouncePenaltyInt, 10000);

	Mat trajReprAvg;

	bool play = false;
	while (true) {
		if(waitKey(play) == ' ') {
			play = !play;
		}

		float localMaximumVar = stdDevIntToVar(localMaximumStdDevInt);
		float positionModelVar = stdDevIntToVar(positionModelStdDevInt);
		float velocityModelVar = stdDevIntToVar(velocityModelStdDevInt);

		auto ballDensity = densityEstimator->next();
		auto density = ballDensity.density;
		auto internals = densityEstimator->getLastInternals();

		Mat dilatedDensity;
		dilate(density, dilatedDensity, Mat(), Point(-1, -1), 10);

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
		localMaxima.resize(min(localMaxima.size(), size_t(localMaximaLimit)));

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
		spawn.velocityVariance = stdDevIntToVar(spawnVelocityStdDevInt);
		spawn.positionVelocityCovariance = 0;

		spawn.weight = - spawnPenaltyInt / 100.f;

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
				bounce.velocityVariance = stdDevIntToVar(spawnVelocityStdDevInt);
				bounce.weight -= bouncePenaltyInt / 100.f;
				bounce.bounced = true;

				currentStates.push_back(bounce);
			}
		}

		sort(currentStates.begin(), currentStates.end(), [](StateDensityComponent a, StateDensityComponent b) {
			return a.weight > b.weight;
		});

		previousStates.clear();
		for(auto s : currentStates) {
			if(previousStates.size() >= statesLimit) {
				break;
			}

			if(!Rect_<int>(Point(0, 0), density.size()).contains(Point(s.position[0], s.position[1]))) {
				continue;
			}


			bool dropped = false;
			for(auto ps : previousStates) {
				if(
						norm(s.position - ps.position) < 5 &&
						norm(s.velocity - ps.velocity) < 1 &&
						abs(s.velocityVariance - ps.velocityVariance) < 0.2) {
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

		trajReprAvg = 0.8 * trajReprAvg + 0.5 * trajRepr;
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
