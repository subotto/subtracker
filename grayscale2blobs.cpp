#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <string>
#include <iostream>

#include <opencv2/core/core.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;
using namespace std;


class Blob {
	public:
		int x;
		int y;
		float probability;
	
		Blob(int x, int y, float probability)
			: x(x), y(y), probability(probability)
		{};
	
	private:
		
};

bool operator< (Blob const &a, Blob const &b) {
	return (a.probability > b.probability);
}

vector<Blob> localmaxima;
vector<Blob> result;
float image[500][500];

vector<Blob> grayscale2blobs( float img[][500], int w, int h ) {
	
	localmaxima.clear();
	
	for (int i=1; i<h-1; ++i) {
		for (int j=1; j<w-1; ++j) {
			float v = img[i][j];
			if ( v > img[i-1][j-1] && v > img[i-1][j] && v > img[i-1][j+1] && v > img[i][j-1] && v >= img[i][j+1] && v >= img[i+1][j-1] && v >= img[i+1][j] && v >= img[i+1][j+1] ) {
				localmaxima.push_back( Blob(i,j,v) );
			}
		}
	}
	
	int k = 5;
	if ( k > localmaxima.size() ) k = localmaxima.size();
	nth_element( localmaxima.begin(), localmaxima.begin()+5, localmaxima.end() );
	
	while ( localmaxima.size() > k ) localmaxima.pop_back();
	
	return localmaxima;
	
}


int main(int argc, char* argv[]) {
	
	string videoName;
	if (argc == 2) {
		videoName = argv[1];
	} else {
		cerr << "Usage: " << argv[0] << " <video>" << endl;
		return 1;
	}

	VideoCapture cap;
	cap.open(videoName);
	
	namedWindow("output1", CV_WINDOW_NORMAL);
	
	int w = 320;
	int h = 240;
	
	while(waitKey(0)) {
		Mat frame;
		Mat grayframe;
		
		cap >> frame;
		
		cvtColor(frame, grayframe, CV_BGR2GRAY);
		
		imshow("output1", frame);
		
		w = grayframe.cols;
		h = grayframe.rows;
		
		printf("%d %d\n", w,h);
		
		for (int i=0; i<h; ++i) {
			for (int j=0; j<w; ++j) {
				float v = grayframe.at<float>(i,j);
				// printf("%f\n", v);
				image[i][j] = v;
			}
		}
		
		result = grayscale2blobs(image, w, h);
		
		for (int i=0; i<result.size(); i++) {
			printf("(%d,%d), probability=%f\n", result[i].x, result[i].y, result[i].probability);
		}
	}
	return 0;
}
