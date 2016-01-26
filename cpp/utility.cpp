#include "utility.hpp"

#include "jpegreader.hpp"

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

// Courtesy of http://stackoverflow.com/a/2072890/807307
bool ends_with(std::string const & value, std::string const & ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

FrameCycle *open_frame_cycle(string videoName, control_panel_t &panel, int width, int height) {

  FrameCycle *f;
  if(videoName.size() == 1) {
    f = new FrameReader(videoName[0] - '0', panel);
  } else if (videoName.back() == '~') {
    videoName = videoName.substr(0, videoName.size()-1);
    bool from_file;
    bool simulate_live;
    JPEGReader::mangle_file_name(videoName, from_file, simulate_live);
    f = new JPEGReader(videoName, panel, from_file, simulate_live, width, height);
  } else {
    bool simulate_live = false;
    if(videoName.back() == '+') {
      videoName = videoName.substr(0, videoName.size()-1);
      simulate_live = true;
    }
    f = new FrameReader(videoName.c_str(), panel, simulate_live);
  }
  return f;

}
