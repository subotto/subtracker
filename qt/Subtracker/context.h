#ifndef CONTEXT_H
#define CONTEXT_H

#include "framesettings.h"
#include "frameanalysis.h"
#include "framereader.h"

#include <chrono>
#include <vector>
#include <mutex>
#include <atomic>
#include <set>
#include <condition_variable>

#include <opencv2/core/core.hpp>

class Context
{
public:
    Context(int slave_num, FrameProducer *producer);
    void feed(const cv::Mat &frame, std::chrono::time_point< std::chrono::system_clock > playback_time);
    FrameAnalysis *get();
    FrameAnalysis *maybe_get();
    ~Context();

private:
    void phase1_fn(FrameAnalysis *frame);
    void phase2_fn(FrameAnalysis *frame);
    void phase3_fn(FrameAnalysis *frame);

    void phase1_thread();
    void phase2_thread();
    void phase3_thread();

    std::atomic< bool > running;
    std::vector< std::thread > slaves;

    std::mutex phase1_mutex, phase2_mutex, phase3_mutex;
    std::condition_variable phase1_empty, phase2_empty, phase3_empty, phase1_full, phase2_full, phase3_full;
    FrameAnalysis *phase1_out, *phase2_out, *phase3_out;

    // phase1 variables
    int frame_num;
    FrameSettings settings;
    FrameProducer *producer;

    // phase3 variables
    int phase3_frame_num;
    std::set< std::pair< int, FrameAnalysis* > > reorder_buf;
};

#endif // CONTEXT_H
