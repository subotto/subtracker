#include "frameanalysis.h"

using namespace std;
using namespace chrono;
using namespace cv;

FrameAnalysis::FrameAnalysis(const cv::Mat &frame, int frame_num, const std::chrono::time_point< std::chrono::system_clock > &time, const FrameSettings &settings) :
    frame(frame), frame_num(frame_num), time(time), settings(settings) {

}

void FrameAnalysis::phase1() {
    this->begin_time = system_clock::now();
    this->begin_phase1 = steady_clock::now();

    this->test_phase1 = this->frame;

    this->end_phase1 = steady_clock::now();
}

void FrameAnalysis::phase2() {
    this->begin_phase2 = steady_clock::now();

    this->test_phase2 = this->test_phase1;

    this->end_phase2 = steady_clock::now();
}

void FrameAnalysis::phase3() {
    this->begin_phase3 = steady_clock::now();

    this->test_phase3 = this->test_phase2;

    this->end_phase3 = steady_clock::now();
    this->end_time = system_clock::now();
}

std::chrono::steady_clock::duration FrameAnalysis::total_processing_time() {
    return (this->end_phase1 - this->begin_phase1) + (this->end_phase2 - this->begin_phase2) + (this->end_phase3 - this->begin_phase3);
}
