#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/video/tracking.hpp>
#include <opencv2/core/core.hpp>
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include <opencv2/opencv.hpp>

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <list>

using namespace cv;
using namespace std;

Mat old_frame,new_frame,displayedImage;
Mat fore,back;
int maxPoint = 1000;
vector <Point2f> corners;
vector <uchar> status;

void add_points()
{
  Mat gray;
  cvtColor(new_frame,gray,CV_BGR2GRAY);
  goodFeaturesToTrack(gray,corners,maxPoint,0.01,10,Mat(),3,false);
  cout << "Number of corners: " << corners.size() << endl;
}

void draw_points()
{
  for (int i = 0; i < corners.size() ; ++i)
    {
      circle(displayedImage,corners[i],2,Scalar(0,0,255),-1,8,0);
    }
  
  imshow("output1",displayedImage);
  waitKey(20);
}

int main( int argc, char** argv )
{

  VideoCapture cap = VideoCapture(argv[1]);
  if (!cap.isOpened())
    {
      cout << "fava" << endl;
    }


  
  int motionthresh = 40;
  
  namedWindow ("output1",CV_WINDOW_NORMAL);
  namedWindow ("output2",CV_WINDOW_NORMAL);
  createTrackbar("thresh","output2",&motionthresh,255, NULL);

  cap >> new_frame;



  cout << new_frame.size() << endl;
  Mat rettangolo = Mat::zeros(new_frame.size(),new_frame.type());
  if (0)
  {
    Scalar color = Scalar(0,0,255);
    line (rettangolo,Point(100,100),Point(100,600),color,3);
    line (rettangolo,Point(100,600),Point(1000,600),color,3);
    line (rettangolo,Point(1000,600),Point(1000,100),color,3);
    line (rettangolo,Point(1000,100),Point(100,100),color,3);

  }

  


  new_frame.copyTo(displayedImage);

  add_points();
  draw_points();
  while (1)
  {
    new_frame.copyTo(old_frame);
    cap >> new_frame;

    if (new_frame.empty())
      break;
    new_frame.copyTo(displayedImage);
    //    cvtColor(new_frame,new_frame,CV_BGR2GRAY);
    if (corners.empty())
      {
	add_points();
	draw_points();
	continue;
      }
    GaussianBlur(new_frame,new_frame,Size(5,5),0,0);
    vector<float> err;
    vector < Point2f > temp_points;
    calcOpticalFlowPyrLK(old_frame,new_frame,corners,temp_points,status,err);

    Mat trasformazione = findHomography(corners,temp_points);
    Mat temp2,temp3;
    warpPerspective(rettangolo,temp2,trasformazione,rettangolo.size());
    rettangolo = temp2;
    displayedImage += rettangolo;
    warpPerspective(old_frame,temp3,trasformazione,old_frame.size());
    Mat motion;
    absdiff(temp3,new_frame,motion);
    threshold(motion, motion, motionthresh, 255,THRESH_BINARY);
    imshow("output2",motion);

    corners.clear();
    for (int i = 0; i < temp_points.size(); ++i)
      {
	if (status[i])
	  {
	    corners.push_back(temp_points[i]);
	  }
      }
    draw_points();

    if (corners.size() < maxPoint /2 )
      {
	corners.clear();
	add_points();
      }
  }

  return 0;
}
