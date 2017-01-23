#ifndef COORDINATES_H
#define COORDINATES_H

#include <opencv2/core/core.hpp>
#include <vector>

#include "frameanalysis.h"

/* We have the following coordinates set:
 *  * Frame coordinates: the pixel coordinates on the frame received from the camera;
 *  * Table frame coordinates: the pixel coordinates on the reprojected frame;
 *  * Physical coordinates: the physical coordinates, aligned and centered with the table, expressed in meters;
 *  * Work coordinates: physical coordinates rescaled so that the x axis ranges from -1 to +1.
 *
 * Conversions between coordinates is performed identifying the table rectangle in all of them and then fitting an homography on the four corners.
 */

typedef std::vector< cv::Point2f > RefRect;

// This is actually a noop, but we keep it for completeness
inline const RefRect &compute_frame_rectangle(const std::vector< cv::Point2f > &corners) {
    return corners;
}

inline RefRect compute_table_frame_rectangle(const cv::Size2i &size) {
    float width = (float) size.width - 1;
    float height = (float) size.height - 1;
    return {{ 0, height }, { width, height }, { width, 0 }, { 0, 0 }};
}

inline RefRect compute_physical_rectangle(const FrameSettings &settings) {
    return {{ -settings.table_length/2, -settings.table_width/2 },
            { +settings.table_length/2, -settings.table_width/2 },
            { +settings.table_length/2, +settings.table_width/2 },
            { -settings.table_length/2, +settings.table_width/2 }};
}

inline RefRect compute_work_rectangle(const FrameSettings &settings) {
    float ratio = settings.table_width / settings.table_length;
    return {{ -1, -ratio },
            { +1, -ratio },
            { +1, +ratio },
            { -1, +ratio }};
}

inline float avg(float a, float b) {
    return (a+b)/2.0;
}

inline cv::Size2f compute_intermediate_size(const FrameSettings &settings) {
    float alpha_root = sqrt(settings.intermediate_alpha);
    return cv::Size2f(alpha_root * avg(norm(settings.ref_corners[0] - settings.ref_corners[1]), norm(settings.ref_corners[2] - settings.ref_corners[3])),
                      alpha_root * avg(norm(settings.ref_corners[0] - settings.ref_corners[3]), norm(settings.ref_corners[1] - settings.ref_corners[2])));
}

cv::Point2f transform_point(const cv::Point2f &p, const RefRect &from, const RefRect &to);
std::pair< cv::Point2f, cv::Point2f > transform_pair(const std::pair<cv::Point2f, cv::Point2f> &p, const RefRect &from, const RefRect &to);

double rod_x(const FrameSettings &settings, uint8_t rod);
double foosman_y(const FrameSettings &settings, uint8_t rod, uint8_t num);
std::pair< cv::Point2f, cv::Point2f > rod_coords(const FrameSettings &settings, uint8_t rod);
cv::Point2f foosman_coords(const FrameSettings &settings, uint8_t rod, uint8_t num);

#endif // COORDINATES_H
