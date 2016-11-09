#ifndef FRAMEANALYSIS_H
#define FRAMEANALYSIS_H

#include <chrono>
#include <opencv2/core/core.hpp>

#include "framesettings.h"

class FrameAnalysis
{
    friend class Context;
    friend class MainWindow;

public:
    FrameAnalysis(const cv::Mat &frame, int frame_num, const std::chrono::time_point< std::chrono::system_clock > &time, const FrameSettings &settings);
    std::chrono::steady_clock::duration total_processing_time();

private:
    void phase1();
    void phase2();
    void phase3();

    cv::Mat frame;
    int frame_num;
    std::chrono::time_point< std::chrono::system_clock > time;
    FrameSettings settings;

    std::chrono::time_point< std::chrono::system_clock > begin_time, end_time;
    std::chrono::time_point< std::chrono::steady_clock > begin_phase1, end_phase1, begin_phase2, end_phase2, begin_phase3, end_phase3;

    cv::Mat warping;
};

#endif // FRAMEANALYSIS_H
