
#include "coordinates.h"

#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace cv;

double rod_x(const FrameSettings &settings, float rod) {
    return (rod - 0.5 * (settings.rod_num-1)) * settings.rod_distance;
}

double foosman_y(const FrameSettings &settings, uint8_t rod, float fm) {
    return (fm - 0.5 * (settings.rod_configuration[rod].num-1)) * settings.rod_configuration[rod].dist;
}

std::pair<cv::Point2f, cv::Point2f> rod_coords(const FrameSettings &settings, float rod)
{
    float x = rod_x(settings, rod);
    float y1 = -0.5 * settings.table_width;
    float y2 = 0.5 * settings.table_width;
    return make_pair(Point2f(x, y1), Point2f(x, y2));
}

cv::Point2f foosman_coords(const FrameSettings &settings, uint8_t rod, float fm) {
    float x = rod_x(settings, rod);
    float y = foosman_y(settings, rod, fm);
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

double convert_phys_to_table_frame_x(const FrameSettings &settings, const Size2i &size, double phys) {
    return transform_point(Point2f(phys, 0.0), compute_physical_rectangle(settings), compute_table_frame_rectangle(size)).x;
}

double convert_phys_to_table_frame_y(const FrameSettings &settings, const Size2i &size, double phys) {
    return transform_point(Point2f(0.0, phys), compute_physical_rectangle(settings), compute_table_frame_rectangle(size)).y;
}

double convert_table_frame_to_phys_x(const FrameSettings &settings, const Size2i &size, double phys) {
    return transform_point(Point2f(phys, 0.0), compute_table_frame_rectangle(size), compute_physical_rectangle(settings)).x;
}

double convert_table_frame_to_phys_y(const FrameSettings &settings, const Size2i &size, double phys) {
    return transform_point(Point2f(0.0, phys), compute_table_frame_rectangle(size), compute_physical_rectangle(settings)).y;
}

double table_frame_rod_x(const FrameSettings &settings, const Size2i &size, float rod)
{
    double phys = rod_x(settings, rod);
    return convert_phys_to_table_frame_x(settings, size, phys);
}

double table_frame_foosman_y(const FrameSettings &settings, const Size2i &size, uint8_t rod, float fm)
{
    double phys = foosman_y(settings, rod, fm);
    return convert_phys_to_table_frame_y(settings, size, phys);
}
