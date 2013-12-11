#include <stdio.h>
#include <iostream>
#include <vector>
#include "median.hpp"

using namespace cv;
using namespace std;

int main(int argc, char** argv) {
    VideoCapture capture(argv[1]);
    if(!capture.isOpened()){
        cerr << "Unable to open video file:" << argv[1] << endl;
        exit(EXIT_FAILURE);
    }
    namedWindow("Frame", WINDOW_NORMAL);
    namedWindow("Median", WINDOW_NORMAL);
    Mat frame;
    MedianComputer mc(240, 320);
    while(waitKey(1) != 'e') {
        if(!capture.read(frame)) {
            cerr << "Video end." << endl;
            break;
        }
        mc.update(frame);
        imshow("Frame", frame);
        imshow("Median", mc.getMedian());
    }
    cerr << "Capture ended" << endl;
    destroyAllWindows();
    capture.release();
}
