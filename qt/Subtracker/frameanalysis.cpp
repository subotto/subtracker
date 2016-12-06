#include "frameanalysis.h"
#include "tabletracking.h"

#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace chrono;
using namespace cv;

FrameAnalysis::FrameAnalysis(const cv::Mat &frame, int frame_num, const std::chrono::time_point< std::chrono::system_clock > &time, const std::chrono::time_point< std::chrono::system_clock > &acquisition_time, const std::chrono::time_point<steady_clock> &acquisition_steady_time, const FrameSettings &settings, const std::vector<FrameCommands> &commands) :
    frame(frame), frame_num(frame_num), time(time), acquisition_time(acquisition_time), acquisition_steady_time(acquisition_steady_time), settings(settings), commands(commands) {

}

void FrameAnalysis::compute_objects_ll(int color) {
    Mat tmp;
    Mat diff = this->float_table_frame - this->settings.objects_colors[color];
    float factor = -0.5f / (this->settings.objects_color_stddev[color] * this->settings.objects_color_stddev[color]);
    Matx< float, 1, 3 > t = { factor, factor, factor };
    transform(diff.mul(diff), tmp, t);
    this->objects_ll[color] = tmp - 3.0f/2.0f * log(2 * M_PI * this->settings.objects_color_stddev[color]);
    this->objects_ll[color].convertTo(this->viz_objects_ll[color], CV_8UC1, 10.0, 255.0);
}

void FrameAnalysis::track_table(FrameContext &frame_ctx, ThreadContext &thread_ctx)
{
    BFMatcher matcher;
}

void FrameAnalysis::do_things(FrameContext &frame_ctx, ThreadContext &thread_ctx)
{
    (void) frame_ctx;
    (void) thread_ctx;

    this->begin_steady_time = steady_clock::now();
    this->begin_time = system_clock::now();

    this->ref_image = this->settings.ref_image;
    this->ref_mask = this->settings.ref_mask;

    this->track_table(frame_ctx, thread_ctx);

    Size &size = this->settings.intermediate_size;
    Point2f image_corners[4] = { { 0.0, (float) size.height },
                                 { (float) size.width, (float) size.height },
                                 { (float) size.width, 0.0 },
                                 { 0.0, 0.0 } };
    Mat trans = getPerspectiveTransform(image_corners, this->settings.table_corners);
    warpPerspective(this->frame, this->table_frame, trans, size, INTER_NEAREST | WARP_INVERSE_MAP);

    this->table_frame_on_main = frame.clone();
    for (int i = 0; i < 4; i++) {
        line(this->table_frame_on_main, this->settings.table_corners[i], this->settings.table_corners[(i+1)%4], Scalar(0, 0, 255), 2);
    }

    this->table_frame.convertTo(this->float_table_frame, CV_32FC3, 1.0/255.0);

    for (int color = 0; color < 3; color++) {
        this->compute_objects_ll(color);
    }

    // phase 3
    this->end_steady_time = steady_clock::now();
    this->end_time = system_clock::now();
}

std::chrono::steady_clock::duration FrameAnalysis::total_processing_time() {
    return this->end_steady_time - this->begin_steady_time;
}

ThreadContext::ThreadContext() : tj_dec(tjInitDecompress())
{
}

ThreadContext::~ThreadContext()
{
    tjDestroy(this->tj_dec);
}
