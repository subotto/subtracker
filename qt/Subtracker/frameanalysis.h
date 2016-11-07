#ifndef FRAMEANALYSIS_H
#define FRAMEANALYSIS_H

#include <chrono>
#include <opencv2/core/core.hpp>

#include "framesettings.h"

class FrameAnalysis
{
public:
    FrameAnalysis(const cv::Mat &frame, int frame_num, const std::chrono::time_point< std::chrono::system_clock > &time, const FrameSettings &settings);
    void process();

private:
    cv::Mat frame;
    int frame_num;
    std::chrono::time_point< std::chrono::system_clock > time;
    FrameSettings settings;

    // Just to test
    cv::Mat frame_copy;
};

#endif // FRAMEANALYSIS_H
