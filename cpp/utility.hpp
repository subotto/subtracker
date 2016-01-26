#ifndef UTILITY_HPP_
#define UTILITY_HPP_

#include <sstream>
#include <string>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "framereader.hpp"

bool ends_with(std::string const & value, std::string const & ending);

/* width and height are only used when invoking JPEGReader */
FrameCycle *open_frame_cycle(string videoName, control_panel_t &panel, int width=-1, int height=-1);

#endif /* UTILITY_HPP_ */
