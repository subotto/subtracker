#include "blobs_finder.hpp"

#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/photo/photo.hpp"

#include <iostream>
#include <vector>

using namespace cv;
using namespace std;

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

int main(int argc, char* argv[])
{

	string filename;
	if (argc == 2) {
		filename = argv[1];
	} else {
		cerr << "No filename specified." << endl;
		return 1;
	}

	BlobsFinder tracker;
	tracker.Init(filename);
  
	KFInit();
	
	namedWindow("Display",CV_WINDOW_NORMAL);
	
	while (true) {
        char c = (char)waitKey(30);
        if( c == 27 )
            break;
        if (c=='n') {
			vector<Blob> blobs=tracker.ProcessNextFrame();
			display=tracker.PopFrame();
			//DrawBlobs(blobs);
			Point2f estimated=KFTrack(blobs);
			circle( display, estimated, 2, Scalar(0,255,0), -1, 8, 0 );
			imshow("Display", display);
        }
	}
	
	return 0;
}
