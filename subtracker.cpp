#include "blobs_finder.hpp"

#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/photo/photo.hpp"

#include <iostream>
#include <vector>
#include <deque>


using namespace cv;
using namespace std;

double const INFTY = 1e100;

class Node {
	
	public:
		Blob blob;
		double badness;
		int time;
		
		Node(Blob blob, int time)
			: blob(blob), badness(INFTY), time(time)
		{};
	
};





Mat display;

KalmanFilter KF(4, 2, 0);

void DrawBlobs(vector<Blob>& blobs) {
	
	for (int i=0; i<blobs.size(); i++) {
		circle( display, blobs[i].center, blobs[i].radius, Scalar(0,255,0), 1, 8, 0 );
	}
	
}

void KFInit() {
	double delta_t=1;
	double sigma_a=1;
	double sigma_m=10;
	
	KF.transitionMatrix = *(Mat_<float>(4, 4) << 1,0,1,0,   0,1,0,1,  0,0,1,0,  0,0,0,1);
	
    setIdentity(KF.measurementMatrix);
	setIdentity(KF.processNoiseCov, Scalar::all(1e-1));
	//setIdentity(KF.measurementNoiseCov, Scalar::all(1e-1));
	setIdentity(KF.measurementNoiseCov, Scalar::all(1));
	setIdentity(KF.errorCovPost, Scalar::all(.1));
}

Point2f KFTrack(vector<Blob>& blobs) {
	Mat prediction = KF.predict();
	
	/* Needed if no correction is made */
	KF.statePre.copyTo(KF.statePost);
    KF.errorCovPre.copyTo(KF.errorCovPost);
	
	if (blobs.empty())
		return Point2f(prediction.at<float>(0), prediction.at<float>(1));
	
	Point2f predicted_position(prediction.at<float>(0), prediction.at<float>(1));
	
	int min_index=-1;
	double min_distance=-1;
	for (int i=0; i<blobs.size(); i++) {
		double distance=norm(blobs[i].center - predicted_position);
		if (distance < min_distance || min_distance<0) {
			min_index=i;
			min_distance=distance;
		}
	}
	
	
	Mat estimated = KF.correct(Mat(blobs[min_index].center));
	
	return Point2f(estimated.at<float>(0), estimated.at<float>(1));
}


deque< vector<Node> > timeline;

void InsertFrameInTimeline(vector<Blob> blobs, int time) {
	vector<Node> v;
	for (int i=0; i<blobs.size(); i++) {
		Node n = Node( blobs[i], time );
		v.push_back(n);
	}
	timeline.push_back(v);
}

Node ProcessFrame() {
	
	vector<Node> &last = timeline.back();
	
	if ( last.empty() ) {
		return Node( Blob( Point2f(0.0,0.0), 0.0, 0.0 ), 0 );
	}
	
	for (int k=0; k < last.size(); k++) {
		for (deque< vector<Node> >::iterator i=timeline.begin(); i!=timeline.end(); i++) {
			
			vector<Node> &old = *i;
			
			for (int j=0; j < i->size(); j++) {
				
				int interval = last[k].time - old[j].time;
				double old_badness = old[j].badness;
				
				double new_badness = old_badness + ( interval - 1 );
				
				if ( new_badness < last[k].badness ) {
					last[k].badness = new_badness;
					// TODO: salvare il percorso ottimo
				}
				
			}
		}
	}
	
	double min_badness = INFTY;
	int best_node_index = 0;
	
	for (int k=0; k < last.size(); k++) {
		if ( min_badness < last[k].badness ) {
			
			best_node_index = k;
			min_badness = last[k].badness;
			
		}
	}
	
	return last[best_node_index];
}


int main(int argc, char* argv[])
{

	string filename;
	if (argc == 2) {
		filename = argv[1];
	} else {
		cerr << "No filename specified." << endl;
		return 1;
	}

	BlobsFinder blobs_finder;
	blobs_finder.Init(filename);
  
	KFInit();
	
	namedWindow("Display",CV_WINDOW_NORMAL);
	
	int time = 0;
	
	while (true) {
        char c = (char)waitKey(30);
        if( c == 27 )
            break;
        if (c=='n') {
			vector<Blob> blobs=blobs_finder.ProcessNextFrame();
			display=blobs_finder.PopFrame();
			
			InsertFrameInTimeline(blobs, time++);
			
			Node ball = ProcessFrame();
			
			DrawBlobs(blobs);
			//Point2f estimated=KFTrack(blobs);
			Point2f estimated = ball.blob.center;
			circle( display, estimated, 2, Scalar(0,255,0), -1, 8, 0 );
			imshow("Display", display);
        }
	}
	
	return 0;
}
