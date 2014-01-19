#include <iostream>
#include <iterator>
#include <string>
#include <utility>
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

void gioveBallTracking() {
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
	
	BlobsTracker blobs_tracker;
	
	
	int timeline_span = 120;
	int processed_frames = 10;	// number of frames to be processed for each call to ProcessFrame
	
	int current_time = 0;
	int initial_time = 0;
	
	namedWindow( "Display", WINDOW_NORMAL );
	deque<Mat> frames;	// used for display only
	
	while (true) {
		int c = waitKey(play);

		if (c == ' ') {
			play = !play;
		}

		if (c == 'd') {
			debug = !debug;
		}
		
		// Massimo mi passa i punti migliori
		auto ballDensity = densityEstimator->next();
		auto density = ballDensity.density;
		auto internals = densityEstimator->getLastInternals();
		
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
			// printf("Weight: %lf\n", localMaxima[i].weight);
		}
		printf("Inserting frame %d in timeline.\n", current_time);
		blobs_tracker.InsertFrameInTimeline(blobs, current_time);
		
		
		// Metto da parte il frame per l'eventuale visualizzazione
		if ( current_time >= timeline_span ) {
			frames.push_back( internals.input );
		}
		
		
		if ( current_time >= 2*timeline_span ) {
			blobs_tracker.PopFrameFromTimeline();
			int processed_time = initial_time + timeline_span;
			
			// Il processing vero e proprio avviene solo ogni k fotogrammi (k=processed_frames)
			if ( processed_time % processed_frames == 0 ) {
				Point2f ball = blobs_tracker.ProcessFrames( initial_time, processed_time, processed_time + processed_frames );
				
				// Mostro l'ultimo fotogramma tra quelli processati
				for (int i=0; i<processed_frames-1; i++) {
					frames.pop_front();
				}
				Mat display;
				assert(!frames.empty());
				frames.front().copyTo(display);
				frames.pop_front();
				ball.x /= metrics.length / density.cols;
				ball.y /= metrics.width / density.rows;
				circle( display, ball, 2, Scalar(0,255,0), 1 );
				imshow("Display", display);
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

	onChange(0, 0);

	gioveBallTracking();

	return 0;
}
