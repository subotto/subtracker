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

void DrawBlobs(vector<Blob>& blobs) {
	
	for (int i=0; i<blobs.size(); i++) {
		Point2f center(blobs[i].x,blobs[i].y);
		circle( display, center, blobs[i].radius, Scalar(0,255,0), 1, 8, 0 );
	}
	
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
  
	namedWindow("Display",CV_WINDOW_NORMAL);
	
	while (true) {
        char c = (char)waitKey(30);
        if( c == 27 )
            break;
        if (c=='n') {
			vector<Blob> blobs=tracker.ProcessNextFrame();
			display=tracker.PopFrame();
			DrawBlobs(blobs);
			imshow("Display", display);
        }
	}
	
	return 0;
}
