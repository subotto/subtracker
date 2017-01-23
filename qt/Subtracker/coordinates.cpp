
#include "coordinates.h"

#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace cv;

double rod_x(const FrameSettings &settings, uint8_t rod) {
    return (rod - 0.5 * (settings.rod_num-1)) * settings.rod_distance;
}

double foosman_y(const FrameSettings &settings, uint8_t rod, uint8_t num) {
    return (num - 0.5 * (settings.rod_configuration[rod].num-1)) * settings.rod_configuration[rod].dist;
}

std::pair<cv::Point2f, cv::Point2f> rod_coords(const FrameSettings &settings, uint8_t rod)
{
    float x = rod_x(settings, rod);
    float y1 = -0.5 * settings.table_width;
    float y2 = 0.5 * settings.table_width;
    return make_pair(Point2f(x, y1), Point2f(x, y2));
}

cv::Point2f foosman_coords(const FrameSettings &settings, uint8_t rod, uint8_t num) {
    float x = rod_x(settings, rod);
    float y = foosman_y(settings, rod, num);
    return { x, y };
}

Point2f transform_point(const Point2f &p, const RefRect &from, const RefRect &to)
{
    Mat homography = getPerspectiveTransform(from, to);
    vector< Point2f > in = { p };
    vector< Point2f > out;
    perspectiveTransform(in, out, homography);
    return out[0];
}

std::pair<Point2f, Point2f> transform_pair(const std::pair<Point2f, Point2f> &p, const RefRect &from, const RefRect &to)
{
    Mat homography = getPerspectiveTransform(from, to);
    vector< Point2f > in = { p.first, p.second };
    vector< Point2f > out;
    perspectiveTransform(in, out, homography);
    return make_pair(out[0], out[1]);
}
