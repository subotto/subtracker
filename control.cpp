#include "control.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <chrono>

#include <iostream>
#include <iomanip>
#include <fstream>

using namespace cv;
using namespace std;
using namespace chrono;

void init_control_panel(control_panel_t& panel) {
	set_log_level(panel, "control panel", INFO);
}

bool will_show(control_panel_t& panel, string category, string name) {
	return is_toggled(panel, category, SHOW);
}

static void update_show(control_panel_t& panel, string category, string name) {
	show_status_t& status = panel.show_status[category].at(name);

	stringstream window_name_buf;
	window_name_buf << "Display: " << category << " - " << name;
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

static string level_to_string(log_level_t level) {
	switch(level) {
	case DEBUG:
		return "DEBUG";
	case VERBOSE:
		return "VERBOSE";
	case INFO:
		return "INFO";
	case WARNING:
		return "WARNING";
	case ERROR:
		return "ERROR";
	case CRITICAL:
		return "CRITICAL";
	}
}

ostream& logger(control_panel_t& panel, string category, log_level_t level) {
	static ofstream cnull("/dev/null"); // FIXME: its ugly

	auto now = system_clock::now();
	auto now_time = system_clock::to_time_t(now);

	const int max_size = 100;
	char s[max_size];
	strftime(s, max_size, "%d %b %Y %T", localtime(&now_time));

	if(is_loggable(panel, category, level)) {
		return cerr << s << " - " << level_to_string(level) << " - " << category << ": ";
	} else {
		return cnull;
	}
}

void set_log_level(control_panel_t& panel, string category, log_level_t level) {
	panel.log_status[category].level = level;

	logger(panel, "control panel", INFO) << "log level for " << category << " set to: " << level_to_string(level) << endl;
}

bool is_loggable(control_panel_t& panel, string category, log_level_t level) {
	return level >= panel.log_status[category].level;
}

static string togglable_to_string(togglable_t toggable) {
	switch(toggable) {
	case SHOW:
		return "show";
	case TRACKBAR:
		return "trackbar";
	case TIME:
		return "time";
	}
}

void toggle(control_panel_t& panel, string category, togglable_t togglable, int status) {
	auto& toggled = panel.toggle_status[category].toggled[togglable];

	if (status == TOGGLE) {
		toggled = !toggled;
	} else {
		toggled = status;
	}

	logger(panel, "control panel", INFO) <<
			(toggled ? "enabled" : "disabled") << " " << category << " " << togglable_to_string(togglable) << endl;

	for(auto& entry : panel.show_status[category]) {
		update_show(panel, category, entry.first);
	}

	for(auto& entry : panel.trackbar_status[category]) {
		update_trackbar(panel, category, entry.first);
	}
}

bool is_toggled(control_panel_t& panel, string category, togglable_t togglable) {
	auto& toggled = panel.toggle_status[category].toggled[togglable];

	return toggled;
}

void color_picker(control_panel_t& panel, string category, string name, Scalar& color) {
	string bgr_names[] {"blue", "green", "red"};
	for(int k = 0; k < 3; k++) {
		stringstream name_buf;
		name_buf << name << " color " << bgr_names[k];
		trackbar(panel, category, name_buf.str(), color.val[k], {0.f, 1.f, 0.01f});
	}
}

void color_qf_picker(control_panel_t& panel, string category, string name, Matx<float, 1, 6>& qf) {
	string qf_names[] {"bb", "gg", "rr", "bg", "gr", "rb"};
	for(int k = 0; k < 6; k++) {
		stringstream name_buf;
		name_buf << name << " QF " << qf_names[k];
		trackbar(panel, category, name_buf.str(), qf.val[k], {k<3 ? 0.f : -100.f, 100.f, 0.01f});
	}
}
