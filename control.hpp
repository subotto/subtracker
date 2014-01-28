#ifndef CONTROL_HPP_
#define CONTROL_HPP_

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <string>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <memory>

struct show_params_t {
	float contrast = 1.f;
	float brightness = 0.f;
};

struct show_status_t {
	show_params_t params;
	cv::Mat image;
};

enum togglable_t {
	SHOW,
	TIME,
	TRACKBAR,
	TOGGLABLE_END
};

struct toggle_status_t {
	std::atomic<bool> toggled[TOGGLABLE_END];
};

struct time_status_t {
	std::chrono::time_point<std::chrono::high_resolution_clock> last_dump;
};

enum log_level_t {
	DEBUG,
	VERBOSE,
	INFO,
	WARNING,
	ERROR,
	CRITICAL
};

struct log_status_t {
	std::atomic<log_level_t> level {WARNING};
};

template<typename T>
struct trackbar_params_t {
	T start;
	T end;
	T step;
};

struct trackbar_base_status_t {
};

template<typename T>
struct trackbar_type_status_t : public trackbar_base_status_t {
	trackbar_params_t<T> params;
	T& variable;

	trackbar_type_status_t(trackbar_params_t<T> params, T& variable)
	: params(params), variable(variable)
	{}
};

struct control_panel_t;

struct trackbar_status_t {
	control_panel_t& panel;

	std::string category;
	std::string name;
	std::shared_ptr<trackbar_base_status_t> type;

	int value_int;
	int count;
	cv::TrackbarCallback callback;

	template<typename T>
	trackbar_status_t(control_panel_t& panel, std::string category, std::string name, trackbar_params_t<T> params, T& variable)
	: panel(panel), category(category), name(name), type(new trackbar_type_status_t<T>(params, variable))
	{}
};

struct control_panel_t {
	typedef std::unordered_map<std::string, show_status_t> show_status_by_name_t;
	typedef std::unordered_map<std::string, trackbar_status_t> trackbar_status_by_name_t;

	std::unordered_map<std::string, show_status_by_name_t> show_status;
	std::unordered_map<std::string, trackbar_status_by_name_t> trackbar_status;
	std::unordered_map<std::string, toggle_status_t> toggle_status;
	std::unordered_map<std::string, time_status_t> time_status;
	std::unordered_map<std::string, log_status_t> log_status;
};

void init_control_panel(control_panel_t& panel);

void show(control_panel_t& panel, std::string category, std::string name, cv::Mat image, show_params_t params = show_params_t());

bool will_show(control_panel_t& panel, std::string category, std::string name);

void dump_time(control_panel_t& panel, std::string category, std::string name);

std::ostream& logger(control_panel_t& panel, std::string category, log_level_t level = INFO);

bool is_loggable(control_panel_t& panel, std::string category, log_level_t level = INFO);

void set_log_level(control_panel_t& panel, std::string category, log_level_t level);

enum {
	TOGGLE = -1
};

void toggle(control_panel_t& panel, std::string category, togglable_t togglable, int status = TOGGLE);
bool is_toggled(control_panel_t& panel, std::string category, togglable_t togglable);

template <typename T>
static void on_trackbar_change(int value_int, void *status_p) {
	trackbar_status_t& status = *static_cast<trackbar_status_t*>(status_p);

	trackbar_type_status_t<T>& type = static_cast< trackbar_type_status_t<T>& >(*status.type);
	trackbar_params_t<T>& params = type.params;

	std::string category = status.category;
	std::string name = status.name;

	std::stringstream trackbar_name_buf;
	trackbar_name_buf << name;
	std::string trackbar_name = trackbar_name_buf.str();

	std::stringstream window_name_buf;
	window_name_buf << "Trackbars: " << category;
	std::string window_name = window_name_buf.str();

	T value = params.start + value_int * params.step;
	type.variable = value;
//
//	logger(status.panel, "control panel", INFO) <<
//			"changing trackbar " << category << " " << name << " value to: " << value_int << std::endl;
}

static void update_trackbar(control_panel_t& panel, std::string category, std::string name) {
	static int zero;

	trackbar_status_t& status = panel.trackbar_status[category].at(name);

	std::stringstream trackbar_name_buf;
	trackbar_name_buf << name;
	std::string trackbar_name = trackbar_name_buf.str();

	std::stringstream window_name_buf;
	window_name_buf << "Trackbars: " << category;
	std::string window_name = window_name_buf.str();

	if(is_toggled(panel, category, TRACKBAR)) {
		cv::namedWindow(window_name, CV_WINDOW_NORMAL);
		cv::createTrackbar(trackbar_name, window_name, &zero, status.count, status.callback, &status);
		cv::setTrackbarPos(trackbar_name, window_name, status.value_int);
	} else {
		cv::destroyWindow(window_name);
	}
}

template<typename T>
void trackbar(control_panel_t& panel, std::string category, std::string name, T& variable, trackbar_params_t<T> params) {
	panel.trackbar_status[category].emplace(name, trackbar_status_t{panel, category, name, params, variable});

	trackbar_status_t& status = panel.trackbar_status[category].at(name);

	status.value_int = int((variable - params.start) / params.step);
	status.count = int((params.end - params.start) / params.step);
	status.callback = on_trackbar_change<T>;

	update_trackbar(panel, category, name);
}

void color_picker(control_panel_t& panel, std::string category, std::string name, cv::Scalar& color);

void color_qf_picker(control_panel_t& panel, std::string category, std::string name, cv::Matx<float, 1, 6>& qf);

#endif /* CONTROL_HPP_ */
