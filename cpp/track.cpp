#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/photo/photo.hpp"

#include <iostream>
#include <ctype.h>
#include <vector>
#include <cstdlib>
#include <cassert>

using namespace cv;
using namespace std;


class Blob {
	public:
		Blob(double x, double y, double radius, double speed) :
		_x(x), _y(y), _radius(radius), _speed(speed)
		{};
		
		Blob(double x, double y) :
			_x(x), _y(y), _radius(0), _speed(0)
		{};
		
		Blob(double x, double y, double radius) :
			_x(x), _y(y), _radius(radius), _speed(0)
		{};
		
		double x() {return _x;}
		double y() {return _y;}
		double radius() {return _radius;}
		double speed() {return _speed;}
	private:
		double _x;
		double _y;
		double _radius;
		double _speed;
		
};

VideoCapture cap;

Mat raw_frame, old_raw_frame, frame, old_frame;
Mat filtered, displayed, motion_filtered;
Mat first_background;
Mat bg_diff;

vector<vector<Point> > contours;
vector<Vec4i> hierarchy;

int selected_contour_index=1;
vector<Point> selected_contour;
Rect following_window;

int param1=60, param2=15, threshold1=29, threshold2=54;
int saturation_threshold=180, value_threshold=187;
int motion_threshold = 14;
int ball_size=16;

bool erosion=false;

vector<Mat> last_frames;


vector<Blob> blobs;


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

void Motion() {
	Mat img0, img1;
	
	cvtColor(old_frame, img0, CV_BGR2GRAY);
	cvtColor(frame, img1, CV_BGR2GRAY);
	
	GaussianBlur(img0,img0,Size(5,5),0,0);
	GaussianBlur(img1,img1,Size(5,5),0,0);
	absdiff(img0,img1,motion_filtered);
	threshold(motion_filtered, motion_filtered, motion_threshold, 255,THRESH_BINARY);
	imshow("motion", motion_filtered);
	imshow("motion_filter", filtered);
	bitwise_and(motion_filtered, filtered, motion_filtered);
	dilate(motion_filtered, motion_filtered, Mat());
	dilate(motion_filtered, motion_filtered, Mat());
	imshow("motion_filteres", motion_filtered);
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
	
	blur( white_image, white_image, Size(3, 3));
	imshow("blurred", white_image);
	threshold( white_image, white_image, saturation_threshold, 255, 0);

	imshow("value", value);
	blur( value, value, Size(3, 3));
	//bilateralFilter( value, value, 3,6,6);
	imshow("value_denoise", value);
	
	threshold( value, value, value_threshold, 255, 0);
	
	imshow("value_threshold", value);
	
	bitwise_and(white_image, value, filtered);


	if (erosion) {
		erode(filtered, filtered, Mat());
		//erode(filtered, filtered, Mat());
		//dilate(filtered, filtered, Mat());
		//dilate(filtered, filtered, Mat());
		dilate(filtered, filtered, Mat());
	}
	

	cvtColor(filtered, displayed, CV_GRAY2BGR);
	//filtered=saturation;
}

void getFrame() {
	
	if (!old_frame.empty() && !old_raw_frame.empty()) {		
		frame.copyTo(old_frame);
		raw_frame.copyTo(old_raw_frame);
	}
	
	cap >> raw_frame;
	resize(raw_frame, frame, Size(), 1, 1);
	
	if (old_frame.empty() || old_raw_frame.empty()) {
		old_frame = Mat::zeros(frame.size(),frame.type());
		old_raw_frame = Mat::zeros(raw_frame.size(),raw_frame.type());
		
		frame.copyTo(old_frame);
		raw_frame.copyTo(old_raw_frame);
	}
}

void DrawMotionContours(int , void*) {
	
	Mat img=Mat::zeros(motion_filtered.size(),motion_filtered.type());
	motion_filtered.copyTo(img);
	
    vector<vector<Point> > contours0;
    findContours( img, contours0, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);	

    contours.resize(contours0.size());
    for( size_t k = 0; k < contours0.size(); k++ )
        approxPolyDP(Mat(contours0[k]), contours[k], 0.1, true);
        
    vector<vector<Point> > white_contours0;
    findContours( filtered, white_contours0, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);	

	vector<vector<Point> > white_contours;
    white_contours.resize(white_contours0.size());
    for( size_t k = 0; k < white_contours0.size(); k++ )
        approxPolyDP(Mat(white_contours0[k]), white_contours[k], 3, true);
        
    drawContours( displayed, white_contours, -1, Scalar(255,0,255),
                  1, CV_AA, hierarchy, 3 );
    
    
    blobs.clear();
	for( size_t k = 0; k < contours0.size(); k++ ) {
		Rect r=boundingRect(contours0[k]);
		if (r.width > ball_size || r.height > ball_size)
			continue;
	
		Point2f center(r.x+r.width/2,r.y+r.height/2);


		bool in_big_white_blob=false;
		for( size_t h = 0; h < white_contours.size(); h++ ) {
			Rect white_rect=boundingRect(white_contours[h]);
			if (white_rect.width < ball_size || white_rect.height < ball_size)
				continue;
			
			
			double distance = pointPolygonTest(white_contours[h], center, true);
			cout << "distance " << distance << endl;
			
			if (distance > -ball_size/2) {
				in_big_white_blob=true;
				break;
			}
		}
		
		if (in_big_white_blob)
			continue;
	
		rectangle(displayed, r, Scalar(0,255,0), 1);
		blobs.push_back(Blob(center.x, center.y, max(r.height, r.width)));

	}
	
	cout << "Blobs trovate: " << endl;
	for (int i=0; i<blobs.size(); i++) {
		cout << "x: " << blobs[i].x() << " y: " << blobs[i].y() << " radius: " << blobs[i].radius() << endl;
	}
    
    //imshow("Contours", displayed);
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
	Canny(value, edges, threshold1, threshold2);
	//dilate(edges,edges, Mat());
	//Mat img=filtered.clone();
	//bitwise_not(edges,edges);
	//bitwise_and(filtered, edges, img);	
	imshow("edges", edges);
	
	/*Mat img=Mat::zeros(filtered.size(),filtered.type());
	filtered.copyTo(img);
	
    vector<vector<Point> > contours0;
    findContours( img, contours0, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);	

    contours.resize(contours0.size());
    for( size_t k = 0; k < contours0.size(); k++ )
        approxPolyDP(Mat(contours0[k]), contours[k], 0.1, true);
        
    drawContours( displayed, contours, -1, Scalar(0,0,255),
                  1, CV_AA, hierarchy, 3 );
    
	for( size_t k = 0; k < contours0.size(); k++ ) {
		Rect r=boundingRect(contours0[k]);
		if (r.width < ball_size && r.height < ball_size) {
			//contours1.push_back(contours0[k]);
			rectangle(displayed, r, Scalar(0,255,0), 1);
		}
	}
	
	Rect r=boundingRect(contours0[selected_contour_index]);
	rectangle(displayed, r, Scalar(255,0,0), 1);
    */
    //imshow("Contours", displayed);
}

void ReProcess(int , void*) {
	FilterFrame();
	Motion();
	DrawMotionContours(0,NULL);
	//DrawCircles(0, NULL);
	DrawContours(0, NULL);
	imshow("OriginalImage", frame);
	imshow("FilteredImage", filtered);
	imshow("Displayed", displayed);
}

int main(int argc, char* argv[])
{
  // Take filename from command line; if not specified, use the
  // Achille-hardcoded one
  string filename;
  if (argc == 2) {
    filename = argv[1];
  } else {
    filename = "video6.mpg";
  }
  cap.open(filename);
    
    if ( !cap.isOpened() ) {
         cout << "Cannot open the video file" << endl;
         return -1;
    }

    //cap.set(CV_CAP_PROP_POS_MSEC, 7000); //start the video at 300ms
    cap.set(CV_CAP_PROP_POS_FRAMES, 100); //start the video at 300ms
    //cap.set(CV_CAP_PROP_POS_AVI_RATIO, 0.5);
    //cap.set( CV_CAP_PROP_POS_FRAMES, 0.); 
    //for (int i=0; i < 800;i++) {
	//	cap >> frame;
//	}
	cout << "ii " <<cap.get(CV_CAP_PROP_POS_FRAMES) << endl;
	
    double fps = cap.get(CV_CAP_PROP_FPS); //get the frames per seconds of the video
    cout << "Frame per seconds : " << fps << endl;
	
	namedWindow("Slides",CV_WINDOW_NORMAL);
	createTrackbar( "param1", "Slides", &param1, 256, DrawCircles );
	createTrackbar( "param2", "Slides", &param2, 256, DrawCircles );
	createTrackbar( "threshold1", "Slides", &threshold1, 256, DrawContours );
	createTrackbar( "threshold2", "Slides", &threshold2, 256, DrawContours );
	createTrackbar( "saturation_threshold", "Slides", &saturation_threshold, 256, ReProcess );
	createTrackbar( "value_threshold", "Slides", &value_threshold, 256, ReProcess );
	createTrackbar( "motion_threshold", "Slides", &motion_threshold, 256, ReProcess );
	createTrackbar( "ball_size", "Slides", &ball_size, 256, ReProcess );
	
	namedWindow("OriginalImage",CV_WINDOW_NORMAL);
	//namedWindow("Saturation",CV_WINDOW_NORMAL);
	//namedWindow("Value",CV_WINDOW_NORMAL);
	namedWindow("FilteredImage",CV_WINDOW_NORMAL);
	namedWindow("Contours",CV_WINDOW_NORMAL);
	namedWindow("Displayed",CV_WINDOW_NORMAL);
	//namedWindow("ROI",CV_WINDOW_NORMAL);
	
	getFrame();
	ReProcess(0,NULL);
	
	first_background=frame.clone();
	
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
			getFrame();
			ReProcess(0, NULL);
			absdiff(frame, first_background, bg_diff);
			imshow("diff", bg_diff);
            break;
        case 'c':
			DrawContours(0,NULL);
			imshow("Displayed", displayed);
            break;
        case 'v':
			selected_contour_index++;
			DrawContours(0,NULL);
			imshow("Displayed", displayed);
            break;
        case 's':
			selected_contour=contours[selected_contour_index];
			following_window=boundingRect(selected_contour);
            break;
        case 'e':
			erosion=!erosion;
			ReProcess(0, NULL);
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
