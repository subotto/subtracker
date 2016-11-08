#include "frameanalysis.h"

FrameAnalysis::FrameAnalysis(const cv::Mat &frame, int frame_num, const std::chrono::time_point< std::chrono::system_clock > &time, const FrameSettings &settings) :
    frame(frame), frame_num(frame_num), time(time), settings(settings) {

}

void FrameAnalysis::phase1() {
    this->test_phase1 = this->frame;
}

void FrameAnalysis::phase2() {
    this->test_phase2 = this->test_phase1;
}

void FrameAnalysis::phase3() {
    this->test_phase3 = this->test_phase2;
}
