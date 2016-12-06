#ifndef FRAMEANALYSIS_H
#define FRAMEANALYSIS_H

#include <chrono>
#include <opencv2/core/core.hpp>

#include <turbojpeg.h>

#include "framesettings.h"
#include "framewaiter.h"

struct FrameContext {
    bool first_frame = true;
    cv::Mat table_frame_mean;
    cv::Mat table_frame_var;
};

struct ThreadContext {
    ThreadContext();
    ~ThreadContext();
    tjhandle tj_dec;
};

class FrameAnalysis
{
    friend class MainWindow;
    friend class BallPanel;
    friend class FoosmenPanel;
    friend class BeginningPanel;

public:
    FrameAnalysis(const cv::Mat &frame, int frame_num,
                  const std::chrono::time_point< std::chrono::system_clock > &time,
                  const std::chrono::time_point< std::chrono::system_clock > &acquisition_time,
                  const std::chrono::time_point< std::chrono::steady_clock > &acquisition_steady_time,
                  const FrameSettings &settings,
                  const std::vector< FrameCommands > &commands);
    std::chrono::steady_clock::duration total_processing_time();
    void do_things(FrameContext &frame_ctx, ThreadContext &thread_ctx);

private:

    void compute_objects_ll(int color);

    cv::Mat frame;
    int frame_num;
    std::chrono::time_point< std::chrono::system_clock > time, acquisition_time;
    std::chrono::time_point< std::chrono::steady_clock > acquisition_steady_time;
    FrameSettings settings;
    std::vector< FrameCommands > commands;

    std::chrono::time_point< std::chrono::system_clock > begin_time, end_time;
    std::chrono::time_point< std::chrono::steady_clock > begin_steady_time, end_steady_time;

    // phase1
    cv::Mat ref_image, ref_mask;

    // phase2
    cv::Mat table_frame, table_frame_on_main, float_table_frame;
    cv::Mat objects_ll[3];
    cv::Mat viz_objects_ll[3];
};

#endif // FRAMEANALYSIS_H
