#include "blobs_finder.hpp"

#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/photo/photo.hpp"

#include <iostream>

using namespace cv;
using namespace std;

bool BlobsFinder::Init(string filename) {
	_cap.open(filename);
	
	if ( !_cap.isOpened() ) {
         cerr << "Cannot open the video file." << endl;
         return false;
    }
    
    //GetNewFrame();
    Mat frame;
    _cap >> frame;
    _old_frames.push_back(frame);
}

bool BlobsFinder::GetNewFrame() {
	Mat frame;
	
	_cap >> frame;
	if (frame.empty())
		return false;
	
	if (!_frame_buffer.empty())
		_old_frames.push_back(_frame_buffer.back());
		
	if (_old_frames.size() > 2)
		_old_frames.pop_front();

	_frame_buffer.push_back(frame.clone());
	
	return true;
}

Mat BlobsFinder::WhiteMap(Mat frame) {
	Mat white_map;
	Mat hsv, saturation, value;
	vector<Mat> channels;
	
	cvtColor(frame, hsv, CV_BGR2HSV);
	split(hsv, channels);

	saturation=channels[1];
	value=channels[2];

	bitwise_not(saturation, saturation);
	
	blur( saturation, saturation, Size(3, 3));
	threshold( saturation, saturation, _saturation_threshold, 255, 0);

	blur( value, value, Size(3, 3));
	
	threshold( value, value, _value_threshold, 255, 0);
	
	bitwise_and(saturation, value, white_map);


	if (_erosion) {
		erode(white_map, white_map, Mat());
		dilate(white_map, white_map, Mat());
	}
	
	return white_map;
}

Mat BlobsFinder::MovementsMap(Mat old_frame, Mat new_frame) {
	Mat movements_map;
	Mat img0, img1;
	
	cvtColor(old_frame, img0, CV_BGR2GRAY);
	cvtColor(new_frame, img1, CV_BGR2GRAY);
	
	GaussianBlur(img0, img0, Size(5,5), 0, 0);
	GaussianBlur(img1, img1, Size(5,5), 0, 0);
	absdiff(img0, img1, movements_map);

	threshold(movements_map, movements_map, _motion_threshold, 255, THRESH_BINARY);

	return movements_map;
}

Mat BlobsFinder::WhiteMovingMap(Mat white_map, Mat movements_map) {
	Mat white_moving_map;
	bitwise_and(movements_map, white_map, white_moving_map);
	dilate(white_moving_map, white_moving_map, Mat());
	dilate(white_moving_map, white_moving_map, Mat());
	//dilate(white_moving_map, white_moving_map, Mat());
	return white_moving_map;
}

bool BlobsFinder::isPointNearBigWhiteBlob(cv::Mat white_map, cv::Point2f point) {
	Mat img = white_map.clone();
	
	vector<vector<Point> > white_contours0;
    findContours( img, white_contours0, CV_RETR_LIST, CHAIN_APPROX_SIMPLE);	

	vector<vector<Point> > white_contours;
    white_contours.resize(white_contours0.size());
    for( size_t k = 0; k < white_contours0.size(); k++ )
        approxPolyDP(Mat(white_contours0[k]), white_contours[k], 3, true);
        
	for( size_t h = 0; h < white_contours.size(); h++ ) {
		Rect white_rect=boundingRect(white_contours[h]);
		if (white_rect.width < _ball_size && white_rect.height < _ball_size)
			continue;
		
		double distance = pointPolygonTest(white_contours[h], point, true);
		if (distance > -_big_blob_distance) {
			return true;
		}
	}
	
	return false;
}

vector<Blob> BlobsFinder::ProcessNextFrame() {
	vector<Blob> blobs;
	
	if (!GetNewFrame()) {
		cerr << "No more frame to read not managed." << endl;
		return blobs;
	}
	
	/* Create the filtered images. */
	
	Mat white_map = WhiteMap(_frame_buffer.back());
	imshow("white", white_map);
	Mat movements_map = MovementsMap(_frame_buffer.back(), _old_frames.back());
	imshow("movements", movements_map);
	Mat white_moving_map = WhiteMovingMap(white_map, movements_map);
	imshow("white_moving", white_moving_map);
	
	/* Find the blobs */
	Mat img=Mat::zeros(white_moving_map.size(),white_moving_map.type());
	white_moving_map.copyTo(img);
	
    vector<vector<Point> > contours;
    
    findContours( img, contours, CV_RETR_LIST, CHAIN_APPROX_SIMPLE);	
    
	for( size_t k = 0; k < contours.size(); k++ ) {
		Rect r=boundingRect(contours[k]);
		if (r.width > _ball_size || r.height > _ball_size)
			continue;
			
		Point2f center(r.x+r.width/2,r.y+r.height/2);
		
		if (_big_blob_distance_filter && isPointNearBigWhiteBlob(white_map, center))
			continue;
			
		blobs.push_back(Blob(center, max(r.height, r.width)));
	}

	return blobs;	
}

Mat BlobsFinder::PopFrame() {
	Mat first=_frame_buffer.front();
	
	_frame_buffer.pop_front();
	
	if (_frame_buffer.empty()) {
		_old_frames.push_back(first);
	}
	
	return first.clone();
}

