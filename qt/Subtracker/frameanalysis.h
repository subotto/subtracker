#ifndef FRAMEANALYSIS_H
#define FRAMEANALYSIS_H

#include <chrono>
#include <opencv2/core/core.hpp>

#include "framesettings.h"

struct Phase1Context {
    bool first_frame = true;
    cv::Mat table_frame_mean;
    cv::Mat table_frame_var;
};

struct Phase3Context {

};

class FrameAnalysis
{
    friend class Context;
    friend class MainWindow;
    friend class BallPanel;
    friend class FoosmenPanel;
    friend class BeginningPanel;

public:
    FrameAnalysis(const cv::Mat &frame, int frame_num, const std::chrono::time_point< std::chrono::system_clock > &time, const FrameSettings &settings);
    std::chrono::steady_clock::duration total_processing_time();
    std::chrono::steady_clock::duration phase0_time();
    std::chrono::steady_clock::duration phase1_time();
    std::chrono::steady_clock::duration phase2_time();
    std::chrono::steady_clock::duration phase3_time();

private:
    void phase0();
    void phase1(Phase1Context &ctx);
    void phase2();
    void phase3(Phase3Context &ctx);

    void compute_objects_ll(int color);

    cv::Mat frame;
    int frame_num;
    std::chrono::time_point< std::chrono::system_clock > time, acquisitionTime;
    FrameSettings settings;

    std::chrono::time_point< std::chrono::system_clock > begin_time, end_time;
    std::chrono::time_point< std::chrono::steady_clock > begin_phase0, end_phase0, begin_phase1, end_phase1, begin_phase2, end_phase2, begin_phase3, end_phase3;

    // phase1
    cv::Mat ref_image, ref_mask;

    // phase2
    cv::Mat table_frame, table_frame_on_main, float_table_frame;
    cv::Mat objects_ll[3];
    cv::Mat viz_objects_ll[3];
};

#endif // FRAMEANALYSIS_H
