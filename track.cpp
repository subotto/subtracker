#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <iostream>
#include <ctype.h>
#include <vector>
#include <cstdlib>
#include <cassert>

using namespace cv;
using namespace std;

Mat frame, filtered, displayed;

vector<vector<Point> > contours;
vector<Vec4i> hierarchy;

int param1=60, param2=15, treshold1=29, treshold2=54;

vector< pair<Point,double> > old_p;	// Probabilities of the old frame
vector< pair<Point,double> > new_p; // Probabilities of the new frame
vector<double> wp_p;				// White/black probability

double sigma = 1000.0;
double lambda = 0.8;

bool cmp(pair<Point,double> const &a1, pair<Point,double> const &a2) {
	return ( a1.second > a2.second );
}

void DrawCircles(int , void*) {
	Mat frame_gray;
	
	cvtColor(frame, frame_gray, CV_BGR2GRAY);
	GaussianBlur( frame_gray, frame_gray, Size(5, 5), 2, 2 );
	
	//imshow("FilteredImage", frame_gray);
	
	vector<Vec3f> circles;
	HoughCircles( frame_gray, circles, CV_HOUGH_GRADIENT, 1, filtered.rows/4, param1, param2, 13, 20 );
	cout << circles.size() << endl;
	
	cvtColor(filtered, displayed, CV_GRAY2BGR);
	
	
	new_p.clear();
	wp_p.clear();
	double sum = 0.0;	// Sum of wp_p values
	
	for( size_t i = 0; i < circles.size(); i++ ) {
		  Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
		  int radius = cvRound(circles[i][2]);
		  
		  
		  // Find the probability based on the white/black ratio
		  
		  Point corner=center;
		  corner.x-=radius;
		  corner.y-=radius;
		  
		  Rect roi=Rect(corner,Size2f(2*radius, 2*radius));
		  roi=roi & Rect(0,0, filtered.cols, filtered.rows);
		 
		 
		  
		 // cout << "x: " << center.x << " y: " << center.y << " radius: " << radius << endl;
		  //cout << "height: " << 
		  //cout << "x: " << roi.x << " y: " << roi.y
			//<< " w: " << roi.width << " h: " << roi.height << endl;
		  
		  Mat roi_mat=filtered(roi);
		  Mat circle_mat=Mat::zeros(roi_mat.size(), roi_mat.type());
		  
		  circle(circle_mat, Point2f(radius, radius), radius, Scalar(255), -1);
		  
		  bitwise_and(roi_mat, circle_mat, circle_mat);
		  imshow("roi", circle_mat);
		  
		  int nonzero=countNonZero(circle_mat);
		  
		  int circle_area=3.14*radius*radius;
		  
		  cout << "Radius: " << radius << endl;
		  cout << "Non zero: " << nonzero << " area: " << circle_area << endl;
		  
		  double density = ((double)nonzero)/circle_area;
		  if (density > 0.2) {			  
			rectangle(displayed, roi, Scalar(0,255,0));
		  }
		  if (density > 0.6) {			  
			  // circle center
			  circle( displayed, center, 3, Scalar(0,255,0), -1, 8, 0 );
			  
			  // circle center
			  //circle( displayed, center, 3, Scalar(0,255,0), -1, 8, 0 );
			  // circle outline
			  circle( displayed, center, radius, Scalar(0,0,255), 3, 8, 0 );
		  }
	   }
	   
	   
	   
	   
}

void FilterFrame() {
	Mat hsv, saturation, value;
	vector<Mat>  channels;
	
	cvtColor(frame, hsv, CV_BGR2HSV);
	split(hsv, channels);

	saturation=channels[1];
	value=channels[2];
	
	Mat white_image;
	bitwise_not(saturation, white_image);
	threshold( white_image, white_image, 220, 255, 0);
	
	threshold( value, value, 150, 255, 0);
	
	bitwise_and(white_image, value, filtered);

	erode(filtered, filtered, Mat());
	erode(filtered, filtered, Mat());
	dilate(filtered, filtered, Mat());
	dilate(filtered, filtered, Mat());
	dilate(filtered, filtered, Mat());

}

void DrawContours(int , void*) {
	
	Mat hsv, saturation, value;
	vector<Mat>  channels;
	
	cvtColor(frame, hsv, CV_BGR2HSV);
	split(hsv, channels);

	saturation=channels[1];
	value=channels[2];
	
	blur(value, value, Size(3,3));
	
	Mat edges;
	Canny(value, edges, treshold1, treshold2);
	dilate(edges,edges, Mat());
	Mat img=filtered.clone();
	bitwise_not(edges,edges);
	bitwise_and(filtered, edges, img);	
	
	
    vector<vector<Point> > contours0;
    findContours( img, contours0, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);

    contours.resize(contours0.size());
    for( size_t k = 0; k < contours0.size(); k++ )
        approxPolyDP(Mat(contours0[k]), contours[k], 3, true);
        
    drawContours( img, contours, -1, Scalar(128,255,255),
                  3, CV_AA, hierarchy, 3 );
    imshow("Contours", img);
}

int main(int argc, char* argv[])
{
    VideoCapture cap("video6.mpg");
    
    if ( !cap.isOpened() ) {
         cout << "Cannot open the video file" << endl;
         return -1;
    }

    //cap.set(CV_CAP_PROP_POS_MSEC, 7000); //start the video at 300ms
    double fps = cap.get(CV_CAP_PROP_FPS); //get the frames per seconds of the video
    cout << "Frame per seconds : " << fps << endl;
	
	namedWindow("Slides",CV_WINDOW_NORMAL);
	createTrackbar( "param1", "Slides", &param1, 256, DrawCircles );
	createTrackbar( "param2", "Slides", &param2, 256, DrawCircles );
	createTrackbar( "treshold1", "Slides", &treshold1, 256, DrawContours );
	createTrackbar( "treshold2", "Slides", &treshold2, 256, DrawContours );
	
	namedWindow("OriginalImage",CV_WINDOW_NORMAL);
	//namedWindow("Saturation",CV_WINDOW_NORMAL);
	//namedWindow("Value",CV_WINDOW_NORMAL);
	namedWindow("FilteredImage",CV_WINDOW_NORMAL);
	namedWindow("Contours",CV_WINDOW_NORMAL);
	namedWindow("Displayed",CV_WINDOW_NORMAL);
	//namedWindow("ROI",CV_WINDOW_NORMAL);
	
	cap >> frame; 
	FilterFrame();
	DrawCircles(0, NULL);
	
	
	imshow("OriginalImage", frame);
	imshow("FilteredImage", filtered);
	imshow("Displayed", displayed);
	
	
	while (true) {
        char c = (char)waitKey(30);
        if( c == 27 )
            break;
        switch(c)
        {
        case 'n':
			cap >> frame; 
			if (frame.empty())
			{
				break;
			}
			FilterFrame();
			DrawCircles(0, NULL);
			DrawContours(0, NULL);
			imshow("OriginalImage", frame);
			imshow("FilteredImage", filtered);
			imshow("Displayed", displayed);
            break;
        default:
            ;
        }
	}

/*
    namedWindow("MyVideo",CV_WINDOW_AUTOSIZE); //create a window called "MyVideo"

    while(1)
    {
        Mat frame;

        bool bSuccess = cap.read(frame); // read a new frame from video

         if (!bSuccess) //if not success, break loop
        {
                        cout << "Cannot read the frame from video file" << endl;
                       break;
        }

        imshow("MyVideo", frame); //show the frame in "MyVideo" window

        if(waitKey(30) == 27) //wait for 'esc' key press for 30 ms. If 'esc' key is pressed, break loop
       {
                cout << "esc key is pressed by user" << endl; 
                break; 
       }
    }
	*/
    return 0;

}
