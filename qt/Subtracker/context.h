#ifndef CONTEXT_H
#define CONTEXT_H

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
    Context(int slave_num, FrameProducer *producer, const FrameSettings &settings);
    FrameAnalysis *get();
    void wait();
    FrameAnalysis *maybe_get();
    ~Context();
    void set_settings(const FrameSettings &settings);
    bool is_finished();

private:
    void phase0_thread();
    void phase1_thread();
    void phase2_thread();
    void phase3_thread();

    std::atomic< bool > running;
    std::vector< std::thread > slaves;

    std::mutex phase0_mutex, phase1_mutex, phase2_mutex, phase3_mutex;
    std::condition_variable phase0_empty, phase0_full, phase1_empty, phase2_empty, phase3_empty, phase1_full, phase2_full, phase3_full;
    FrameAnalysis *phase0_out, *phase1_out, *phase2_out, *phase3_out;
    std::atomic< int > phase0_count, phase1_count, phase2_count, phase3_count;
    std::atomic< bool > exausted;

    // phase1 variables
    int frame_num;
    std::mutex settings_mutex;
    FrameSettings settings;
    FrameProducer *producer;
    Phase1Context phase1_ctx;

    // phase3 variables
    int phase3_frame_num;
    std::set< std::pair< int, FrameAnalysis* > > reorder_buf;
    Phase3Context phase3_ctx;
};

#endif // CONTEXT_H
