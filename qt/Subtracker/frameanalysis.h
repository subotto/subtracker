#ifndef FRAMEANALYSIS_H
#define FRAMEANALYSIS_H

#include <chrono>
#include <opencv2/core/core.hpp>
#include <opencv2/xfeatures2d.hpp>

#include <turbojpeg.h>

#include "framesettings.h"
#include "framereader.h"
#include "framewaiter.h"

std::string getImgType(int imgTypeInt);

struct FrameContext {
    bool first_frame = true;

    FrameWaiterContext table_tracking_waiter;
    cv::Ptr< cv::xfeatures2d::SURF > surf_detector;
    cv::Mat surf_frame_kps;
    cv::Mat gftt_frame_kps;
    cv::Ptr< cv::GFTTDetector > gftt_detector;
    bool have_fix = false;
    std::vector< cv::Point2f > frame_corners;
    cv::Mat frame_matches;
    cv::Mat ref_image, ref_mask;
    std::vector< cv::KeyPoint > ref_kps;
    cv::Mat ref_descr;
    std::vector< cv::KeyPoint > ref_gftt_kps;

    FrameClock::time_point last_surf;
    FrameClock::time_point last_of;

    FrameWaiterContext table_frame_waiter;
    bool mean_started = false;
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
    friend class DebugPanel;

public:
    FrameAnalysis(const cv::Mat &frame, int frame_num,
                  const std::chrono::time_point< FrameClock > &time,
                  const std::chrono::time_point< std::chrono::system_clock > &acquisition_time,
                  const std::chrono::time_point< std::chrono::steady_clock > &acquisition_steady_time,
                  const FrameSettings &settings,
                  const FrameCommands &commands,
                  FrameContext &frame_ctx,
                  ThreadContext &thread_ctx);
    std::chrono::steady_clock::duration total_processing_time();
    void do_things();
    std::string gen_csv_line() const;
    bool does_have_fix() const;

private:
    void push_debug_frame(cv::Mat &frame);
    void compute_table_ll();
    void compute_objects_ll(int color);
    void track_table();
    void check_table_inversion();
    void find_foosmen();
    void update_mean();
    void find_ball();

    cv::Mat frame;
    int frame_num;
    FrameClockTimePoint time;
    std::chrono::time_point< std::chrono::system_clock > acquisition_time;
    std::chrono::time_point< std::chrono::steady_clock > acquisition_steady_time;
    FrameSettings settings;
    FrameCommands commands;
    FrameContext &frame_ctx;
    ThreadContext &thread_ctx;

    std::chrono::time_point< std::chrono::system_clock > begin_time, end_time;
    std::chrono::time_point< std::chrono::steady_clock > begin_steady_time, end_steady_time;

    bool have_fix;
    cv::Mat ref_image, ref_mask, ref_bn;
    cv::Mat frame_bn, frame_matches;
    std::vector< cv::Point2f > frame_corners;

    cv::Size intermediate_size;
    cv::Mat table_frame, float_table_frame;
    cv::Mat objects_ll[3];
    cv::Mat table_frame_mean;
    cv::Mat table_frame_var;
    cv::Mat table_frame_diff2;
    cv::Mat table_ll;
    std::vector< std::pair< cv::Point2f, float > > spots;

    // Output data
    struct RodPos {
        float shift = 0.0;
        float rot = 0.0;
    };
    bool ball_is_present = false;
    cv::Point2f ball = { 0.0, 0.0 };
    RodPos rods[FrameSettings::rod_num];

    cv::Mat frame_rendering, table_frame_rendering;

    std::vector< cv::Mat > debug;
};

#endif // FRAMEANALYSIS_H
