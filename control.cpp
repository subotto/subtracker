#include "control.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <chrono>

#include <iostream>
#include <iomanip>

using namespace cv;
using namespace std;
using namespace chrono;

bool will_show(control_panel_t& panel, string category, string name) {
	return is_toggled(panel, category, SHOW);
}

static void update_show(control_panel_t& panel, string category, string name) {
	show_status_t& status = panel.show_status[category][name];

	stringstream window_name_buf;
	window_name_buf << category << "-" << name;
	string window_name = window_name_buf.str();

	if(will_show(panel, category, name)) {
		namedWindow(window_name, WINDOW_NORMAL);
		imshow(window_name, status.image);
	} else {
		destroyWindow(window_name);
	}
}

void show(control_panel_t& panel, string category, string name, cv::Mat image, show_params_t params) {
	show_status_t& status = panel.show_status[category][name];

	status.image = image;
	status.params = params;

	update_show(panel, category, name);
}

template<typename T>
void trackbar(control_panel_t& panel, string category, string name, T& variable, trackbar_params_t<T> params) {

}

void dump_time(control_panel_t& panel, string category, string name) {
	time_point<high_resolution_clock> now = high_resolution_clock::now();

	if(panel.time_status.find(category) == panel.time_status.end()) {
		panel.time_status[category].last_dump = now;
	}

	time_point<high_resolution_clock>& last_dump = panel.time_status[category].last_dump;
	double interval_milliseconds = duration_cast<duration<double, milli>>(now - last_dump).count();

	if (is_toggled(panel, category, TIME)) {
		cerr << category << " - " << name << ": ";
		cerr << fixed << setprecision(3) << interval_milliseconds << "ms" << endl;
	}
	last_dump = now;
}

ostream& logger(control_panel_t& panel, string category, log_level_t level) {

}

void toggle(control_panel_t& panel, string category, togglable_t togglable, int status) {
	bool& toggled = panel.toggle_status[category].toggled[togglable];

	if (status == TOGGLE) {
		toggled = !toggled;
	} else {
		toggled = status;
	}

	for(auto entry : panel.show_status[category]) {
		update_show(panel, category, entry.first);
	}
}

bool is_toggled(control_panel_t& panel, string category, togglable_t togglable) {
	bool& toggled = panel.toggle_status[category].toggled[togglable];

	return toggled;
}
