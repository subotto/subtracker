#include <iostream>
#include <string>

#include <opencv2/core/core.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/photo/photo.hpp>
#include <opencv2/videostab/videostab.hpp>

namespace {
	const char* TEMPLATE_FILE = "subotto/subotto.png";
}

using namespace cv;
using namespace std;
using namespace cv::videostab;

int featureCount;

int ransacSize;
int ransacThresh;
int ransacEpsilon;
int ransacProb;

VideoCapture cap;

Mat frame;
Mat templ;

string filename;

void draw_features() {
	GoodFeaturesToTrackDetector fd(featureCount);
	BriefDescriptorExtractor de;

	vector<KeyPoint> template_points, frame_points;

	fd.detect(frame, frame_points);
	fd.detect(templ, template_points);

	Mat template_description, frame_description;

	de.compute(templ, template_points, template_description);
	de.compute(frame, frame_points, frame_description);

	BFMatcher matcher(NORM_HAMMING);

	vector<DMatch> matches;

	matcher.match(template_description, frame_description, matches);

	vector<Point2f> points1, points2;

	for (DMatch match : matches) {
		points1.push_back(template_points[match.queryIdx].pt);
		points2.push_back(frame_points[match.trainIdx].pt);
	}

	RansacParams ransacParams = RansacParams(max(6, ransacSize), ransacThresh / 100.f, ransacEpsilon / 100.f, ransacProb / 100.f);
	Mat motion = estimateGlobalMotionRobust(points1, points2, AFFINE, ransacParams);

	Mat output1;
	drawMatches(templ, template_points, frame, frame_points, matches, output1);

	cout << motion << endl;

	vector<Point2f> subotto(4);
	for(int corner = 0; corner < 4; corner++) {
		float x = corner == 0 || corner == 3 ? 0 : templ.cols;
		float y = corner == 0 || corner == 1 ? 0 : templ.rows;

		Mat point(3, 1, CV_32F);

		point.at<float>(0, 0) = x;
		point.at<float>(1, 0) = y;
		point.at<float>(2, 0) = 1;

		Mat tr;
		invert(motion, tr);
		Mat tranformedPoint = motion*point;
		subotto[corner].x = tranformedPoint.at<float>(0);
		subotto[corner].y = tranformedPoint.at<float>(1);
	}

	Mat output;

	frame.copyTo(output);
	for(int i = 0; i < 4; i++) {
		line(output, subotto[i], subotto[(i+1)%4], Scalar(1));
	}

	imshow("output", output);
	imshow("output1", output1);
}

void on_change(int pos, void* userdata) {
	draw_features();
}

void subotto() {
	cap >> frame;

	namedWindow("slides", CV_WINDOW_AUTOSIZE);
	namedWindow("output", CV_WINDOW_NORMAL);
	namedWindow("output1", CV_WINDOW_NORMAL);

	createTrackbar("featureCount", "slides", &featureCount, 10000, on_change);
	createTrackbar("ransacSize", "slides", &ransacSize, 20, on_change);
	createTrackbar("ransacThresh", "slides", &ransacThresh, 100, on_change);
	createTrackbar("ransacEpsilon", "slides", &ransacEpsilon, 100, on_change);
	createTrackbar("ransacProb", "slides", &ransacProb, 100, on_change);

	draw_features();

	char key;
	while ((key = waitKey(0)) != 23) {
		switch(key) {
		case 'n':
			cap >> frame;
			draw_features();
			break;
		case 'r':
			cap.open(filename);
			cap >> frame;
			draw_features();
			break;
		}
	}
}

int main(int argc, char* argv[]) {
	if (argc == 3) {
		filename = argv[1];
	} else {
		cerr << "Usage: " << argv[0] << " <video> <subotto image>" << endl;
		return 1;
	}

	templ = imread(TEMPLATE_FILE);
	cap.open(filename);

	subotto();

	return 0;
}
