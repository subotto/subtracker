#ifndef CONTROL_HPP_
#define CONTROL_HPP_

#include <opencv2/core/core.hpp>

#include <string>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <atomic>

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

struct control_panel_t {
	typedef std::unordered_map<std::string, show_status_t> show_status_by_name_t;

	std::unordered_map<std::string, show_status_by_name_t> show_status;
	std::unordered_map<std::string, toggle_status_t> toggle_status;
	std::unordered_map<std::string, time_status_t> time_status;
	std::unordered_map<std::string, log_status_t> log_status;
};

void init_control_panel(control_panel_t& panel);

void show(control_panel_t& panel, std::string category, std::string name, cv::Mat image, show_params_t params = show_params_t());

bool will_show(control_panel_t& panel, std::string category, std::string name);

template<typename T>
struct trackbar_params_t {
	T start;
	T end;
	T step;
};

template<typename T>
void trackbar(control_panel_t& panel, std::string category, std::string name, T& variable, trackbar_params_t<T> params);

void dump_time(control_panel_t& panel, std::string category, std::string name);

std::ostream& logger(control_panel_t& panel, std::string category, log_level_t level = INFO);

bool is_loggable(control_panel_t& panel, std::string category, log_level_t level = INFO);

void set_log_level(control_panel_t& panel, std::string category, log_level_t level);

enum {
	TOGGLE = -1
};

void toggle(control_panel_t& panel, std::string category, togglable_t togglable, int status = TOGGLE);
bool is_toggled(control_panel_t& panel, std::string category, togglable_t togglable);

#endif /* CONTROL_HPP_ */
