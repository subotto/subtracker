#ifndef __BLOBS_FINDER_H_
#define __BLOBS_FINDER_H_

#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <string>
#include <vector>
#include <list>

class Blob {
	public:
		cv::Point2f center;
		double radius;
		double speed;
		double weight;
	
		Blob(cv::Point2f center, double radius=0, double speed=0, double weight=0)
			: center(center), radius(radius), speed(speed), weight(weight)
		{};
		
	private:
	
		
};

class BlobsFinder {
	public:
		BlobsFinder()
			: _saturation_threshold(170), _value_threshold(170),
			_motion_threshold(20), _ball_size(16), 
			_erosion(false), _big_blob_distance_filter(false),
			_big_blob_distance(5)
		{};
		
		bool Init(std::string filename);
		std::vector<Blob> ProcessNextFrame();
		cv::Mat PopFrame();
		
	private:
		std::list<cv::Mat> _frame_buffer;
		std::list<cv::Mat> _old_frames;
		cv::VideoCapture _cap;
		cv::Mat _background;
		
		int _saturation_threshold;
		int _value_threshold;
		int _motion_threshold;
		int _ball_size;

		bool _erosion;
		bool _big_blob_distance_filter;
		int _big_blob_distance;
	
		bool GetNewFrame();
		void UpdateBackground(cv::Mat frame, cv::Mat background);
		cv::Mat WhiteMap(cv::Mat frame);
		cv::Mat MovementsMap(cv::Mat old_frame, cv::Mat new_frame);
		cv::Mat WhiteMovingMap(cv::Mat white_map, cv::Mat movements_map);
		bool isPointNearBigWhiteBlob(cv::Mat white_map, cv::Point2f point);
		
};

#endif
