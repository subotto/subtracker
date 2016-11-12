#include "frameanalysis.h"

#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace chrono;
using namespace cv;

FrameAnalysis::FrameAnalysis(const cv::Mat &frame, int frame_num, const std::chrono::time_point< std::chrono::system_clock > &time, const FrameSettings &settings) :
    frame(frame), frame_num(frame_num), time(time), settings(settings) {

}

void FrameAnalysis::phase1() {
    this->begin_time = system_clock::now();
    this->begin_phase1 = steady_clock::now();

    this->end_phase1 = steady_clock::now();
}

void FrameAnalysis::foosmen_ll(int color) {
    Mat diff = this->float_table_frame - this->settings.foosmen_colors[i];
}

void FrameAnalysis::phase2() {
    this->begin_phase2 = steady_clock::now();

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

    this->table_frame.convertTo(this->float_table_frame, CV_32FC3);

    this->foosmen_ll(0);
    this->foosmen_ll(1);

    this->end_phase2 = steady_clock::now();
}

void FrameAnalysis::phase3() {
    this->begin_phase3 = steady_clock::now();

    this->end_phase3 = steady_clock::now();
    this->end_time = system_clock::now();
}

std::chrono::steady_clock::duration FrameAnalysis::total_processing_time() {
    return (this->end_phase1 - this->begin_phase1) + (this->end_phase2 - this->begin_phase2) + (this->end_phase3 - this->begin_phase3);
}
