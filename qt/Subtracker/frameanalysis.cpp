#include "frameanalysis.h"

FrameAnalysis::FrameAnalysis(const cv::Mat &frame, int frame_num, const std::chrono::time_point< std::chrono::system_clock > &time, const FrameSettings &settings) :
    frame(frame), frame_num(frame_num), time(time), settings(settings) {

}

void FrameAnalysis::process() {
    this->frame_copy = this->frame;
}
