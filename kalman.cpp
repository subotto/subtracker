#include "opencv2/video/tracking.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <stdio.h>
#include <iostream>
#include <cmath>

using namespace std;
using namespace cv;


int main(int, char**)
{
    Mat img = Mat::zeros(500, 800, CV_8UC3);
    KalmanFilter KF(4, 2, 0);

    char code = (char)-1;

	double delta_t=1;
	double sigma_a=1;
	double sigma_m=10;
	
	KF.transitionMatrix = *(Mat_<float>(4, 4) << 1,0,1,0,   0,1,0,1,  0,0,1,0,  0,0,0,1);
    
    Mat_<float> measurement(2,1); measurement.setTo(Scalar(0));

	Mat_<float> actual=*(Mat_<float>(4, 1) << 300,300,10,0);

    KF.statePre.at<float>(0) = 300;
    KF.statePre.at<float>(1) = 300;
    KF.statePre.at<float>(2) = 0;
    KF.statePre.at<float>(3) = 0;
    KF.statePost.at<float>(0) = 300;
    KF.statePost.at<float>(1) = 300;
    KF.statePost.at<float>(2) = 0;
    KF.statePost.at<float>(3) = 0;
    
    setIdentity(KF.measurementMatrix);
	setIdentity(KF.processNoiseCov, Scalar::all(1e-1));
	//setIdentity(KF.measurementNoiseCov, Scalar::all(1e-1));
	setIdentity(KF.measurementNoiseCov, Scalar::all(1));
	setIdentity(KF.errorCovPost, Scalar::all(.1));
  
	float t=0;
	for(;;)
	{
		t++;
		
		if ((int)t%30==0) {
			float angle;
			
			angle=theRNG().uniform(0, 100);
			
			actual.at<float>(2)=10*cos(angle);
			actual.at<float>(3)=10*sin(angle);
		}
		
		actual=KF.transitionMatrix*actual;
		
		Mat prediction = KF.predict();
		
		Mat error=Mat::zeros(2,1, CV_32F);
		if ((int)t%13==0 || (int)t%7==0) {
			randn( error, Scalar::all(0), Scalar::all(10));
		}
		else {
			randn( error, Scalar::all(0), Scalar::all(1));
		}
		measurement = KF.measurementMatrix * actual + error;
		
		
		Mat estimated = KF.correct(measurement);

		circle(img, Point2f(measurement.at<float>(0), measurement.at<float>(1)), 2, Scalar(0,0,255), -1);
		circle(img, Point2f(estimated.at<float>(0), estimated.at<float>(1)), 2, Scalar(0,255,255), -1);
		circle(img, Point2f(actual.at<float>(0), actual.at<float>(1)), 2, Scalar(255,0,0), -1);
		
		cout << norm(Point2f(measurement.at<float>(0), measurement.at<float>(1))-Point2f(actual.at<float>(0), actual.at<float>(1))) <<
			 " " << norm(Point2f(estimated.at<float>(0), estimated.at<float>(1))-Point2f(actual.at<float>(0), actual.at<float>(1))) << endl;
		
		
		imshow( "Kalman", img );
		while (true) {
			code = (char)waitKey(100);
			if (code == 27 || code == 'q' || code == 'Q'|| code == 'n') 
				break;
		}
		
		if( code == 27 || code == 'q' || code == 'Q' )
			break;
	}


    return 0;
}
