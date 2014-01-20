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

struct ModelParams {
	float positionVolatility2;
	float velocityVolatility2;
};

struct Observation {
	Vec<float, 2> pos;
	float posVar;
	float weight;
};

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

void doIt() {
	ModelParams model;

	Trackbar<int> localMaximaLimit("track", "localMaximaLimit", 5, 0, 1000);
	Trackbar<float> fps("track", "fps", 120, 0, 2000, 1);
	Trackbar<float> localMaximaMinDistance("track", "localMaximaMinDistance", 5, 0, 200);

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
		if (debug) fprintf(stderr, "Inserting frame %d in timeline.\n", current_time);
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
				vector<Point2f> positions = blobs_tracker.ProcessFrames( initial_time, processed_time, processed_time + processed_frames, debug );
				
				for (int i=0; i<positions.size(); i++) {
					int frame_id = processed_time + i;
					
					// TODO: stampare il vero timestamp del frame frame_id
					double timestamp = 10000.0;
					
					printf("%lf,%lf,%lf\n", timestamp, positions[i].x, positions[i].y);
				}
				
				if (debug) {
					// Mostro il primo fotogramma tra quelli processati
				
					Mat display;
					assert(!frames.empty());
					frames.front().copyTo(display);
					frames.pop_front();
					Point2f ball = positions[0];
					ball.x /= metrics.length / density.cols;
					ball.y /= metrics.width / density.rows;
					circle( display, ball, 2, Scalar(0,255,0), 1 );
					imshow("Display", display);
				}
				else frames.pop_front();
				
				for (int i=0; i<processed_frames-1; i++) {
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

	onChange(0, 0);

	doIt();

	return 0;
}
