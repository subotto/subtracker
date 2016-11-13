#ifndef FRAMESETTINGS_H
#define FRAMESETTINGS_H

#include <opencv2/core/core.hpp>

struct FrameSettings {
    // The four corners of the table, in this order: red defence, red attack, blue defence, blue attack
    cv::Point2f table_corners[4] = { { 100.0, 400.0 }, { 600.0, 400.0 }, { 600.0, 100.0 }, { 100.0, 100.0 } };
    cv::Size intermediate_size = { 400, 300 };
    // OpenCV uses BGR colors
    cv::Scalar foosmen_colors[2] = { { 0.0f, 0.0f, 1.0f}, { 1.0f, 0.0f, 0.0f } };
    float foosmen_color_stddev[2] = { 0.2f, 0.2f };
};

#endif // FRAMESETTINGS_H
