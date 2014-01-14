#include "utility.hpp"

#include <string>
#include <unordered_map>
#include <memory>

using namespace std;
using namespace cv;

unordered_map<string, unique_ptr<int>> showAlpha, showGamma;

void show(string name, Mat image, int initialAlpha, int initialGamma) {
	if(showAlpha.find(name) == showAlpha.end()) {
		showAlpha[name] = unique_ptr<int>(new int(initialAlpha));
		showGamma[name] = unique_ptr<int>(new int(initialGamma));
	}

	float alpha = *showAlpha[name] / 1000.f;
	float gamma = *showGamma[name] / 100.f;
	Mat output = image * alpha + Scalar(gamma, gamma, gamma);

	namedWindow("intensities", CV_WINDOW_NORMAL);
	namedWindow(name, CV_WINDOW_NORMAL);

	createTrackbar(name + "Alpha", "intensities", showAlpha[name].get(), 100000);
	createTrackbar(name + "Gamma", "intensities", showGamma[name].get(), 100);

	imshow(name, output);
}
