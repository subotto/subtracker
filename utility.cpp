#include "utility.hpp"

#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>

using namespace std;
using namespace cv;
using namespace chrono;

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

static unordered_map<string, time_point<high_resolution_clock>> lastByCategory;

void dumpTime(string category, string what) {
	time_point<high_resolution_clock> now = high_resolution_clock::now();

	if(lastByCategory.find(category) == lastByCategory.end()) {
		lastByCategory[category] = now;
	}

	time_point<high_resolution_clock>& last = lastByCategory[category];
	cerr << category << " - " << what << ": " << duration_cast<nanoseconds>(now - last).count() / 1000000.f << endl;
	last = now;
}
