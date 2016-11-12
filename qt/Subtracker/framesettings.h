#ifndef FRAMESETTINGS_H
#define FRAMESETTINGS_H

#include <opencv2/core/core.hpp>

struct FrameSettings {
    // The four corners of the table, in this order: red defence, red attack, blue defence, blue attack
    cv::Point2f table_corners[4];
    cv::Size intermediate_size;
    cv::Point3f foosmen_colors[2];
    float foosmen_color_variance[2];
};

#endif // FRAMESETTINGS_H
