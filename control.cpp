#include "control.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;
using namespace std;

static void update_show(control_panel_t& panel, string category, string name) {
	show_status_t& status = panel.show_status[category][name];

	stringstream window_name_buf;
	window_name_buf << category << "-" << name;
	string window_name = window_name_buf.str();

	namedWindow(window_name, WINDOW_NORMAL);
	imshow(window_name, status.image);
}

bool will_show(control_panel_t& panel, std::string category, std::string name) {
	return true;
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

}

ostream& logger(control_panel_t& panel, string category, log_level_t level) {

}
