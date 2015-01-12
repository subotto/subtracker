#include <iostream>
#include <iterator>
#include <string>
#include <deque>

#include <cmath>
#include <chrono>

#include "utility.hpp"
#include "blobs_finder.hpp"
#include "blobs_tracker.hpp"
#include "subotto_metrics.hpp"
#include "subotto_tracking.hpp"
#include "control.hpp"

using namespace cv;
using namespace std;
using namespace chrono;

control_panel_t panel;

void doIt(FrameReader& frameReader) {

	int wait_key_skip = 5;
	trackbar(panel, "control panel", "update display skip frames", wait_key_skip, {0, 20, 1});

	for (int i = 0; ; i++) {
		panel.update_display = (i % (wait_key_skip + 1) == 0);

		if (panel.update_display) {
			namedWindow("control panel", WINDOW_NORMAL);

			int c = waitKey(1);

			switch(c) {
			case 'r':
				set_log_level(panel, "capture", VERBOSE);
				break;
			case 'p':
				toggle(panel, "control panel", TRACKBAR);
				break;
			case 'f':
				toggle(panel, "frame", SHOW);
				break;
			case 'c':
				toggle(panel, "cycle", TIME);
				break;
			case 's':
				set_log_level(panel, "table detect", VERBOSE);
				toggle(panel, "table detect", TRACKBAR, true);
				break;
			case 'b':
				set_log_level(panel, "ball tracking", VERBOSE);
				toggle(panel, "ball tracking", TRACKBAR, true);
				toggle(panel, "ball tracking", SHOW, true);
				break;
			case 'm':
				toggle(panel, "foosmen tracking", TRACKBAR);
				toggle(panel, "foosmen tracking", SHOW);
				break;
			case 'l':
				toggle(panel, "foosmen metrics", TRACKBAR);
				toggle(panel, "reference metrics", TRACKBAR);
				break;
			case 'n':
				toggle(panel, "foosmen colors", TRACKBAR);
				break;
			case ' ':
				set_log_level(panel, "table detect", WARNING);
				set_log_level(panel, "ball tracking", WARNING);

				toggle(panel, "table detect", TRACKBAR, false);
				toggle(panel, "ball tracking", TRACKBAR, false);
				toggle(panel, "ball tracking", SHOW, false);

				toggle(panel, "foosmen tracking", TRACKBAR, false);
				toggle(panel, "foosmen tracking", SHOW, false);

				toggle(panel, "foosmen metrics", TRACKBAR, false);
				toggle(panel, "reference metrics", TRACKBAR, false);

				toggle(panel, "foosmen colors", TRACKBAR, false);

				toggle(panel, "frame", SHOW, false);
				toggle(panel, "cycle", TIME, false);
				toggle(panel, "control panel", TRACKBAR, false);
				break;
			}

			dump_time(panel, "cycle", "wait key");
		}

		dump_time(panel, "cycle", "start cycle");

		auto frame_info = frameReader.get();
		Mat frame = frame_info.data;

	}

}

int main(int argc, char* argv[]) {

	string videoName;
	if (argc == 2) {
		videoName = argv[1];
	} else {
		cerr << "Usage: " << argv[0] << " <video>" << endl;
		cerr << "<video> can be: " << endl;
		cerr << "\tn (a single digit number) - live capture from video device n" << endl;
		cerr << "\tfilename+ (with a trailing plus) - simulate live capture from video file" << endl;
		cerr << "\tfilename (without a trailing plus) - batch analysis of video file" << endl;
		return 1;
	}

	init_control_panel(panel);
  set_log_level(panel, "gio", VERBOSE);
  //logger(panel, "gio", VERBOSE) << "Test log" << endl;
  //logger(panel, "control panel", VERBOSE);

	if(videoName.size() == 1) {
		FrameReader f(videoName[0] - '0', panel);
		doIt(f);
	} else {
		bool simulate_live = false;

		if(videoName.back() == '+') {
			videoName = videoName.substr(0, videoName.size()-1);
			simulate_live = true;
		}

		FrameReader f(videoName.c_str(), panel, simulate_live);
		doIt(f);
	}

	return 0;

}
