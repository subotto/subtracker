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
    void phase1();
    void phase2();
    void phase3();

private:
    cv::Mat frame;
    int frame_num;
    std::chrono::time_point< std::chrono::system_clock > time;
    FrameSettings settings;

    // Just to test
    cv::Mat test_phase1, test_phase2, test_phase3;
};

#endif // FRAMEANALYSIS_H
