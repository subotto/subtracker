#ifndef CONTEXT_H
#define CONTEXT_H

#include "frameanalysis.h"
#include "framereader.h"
#include "framewaiter.h"

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
    void working_thread();
    template< class Function, class... Args >
    void create_thread(std::string, Function &&f, Args &&... args);

    std::atomic< bool > running;
    std::vector< std::thread > slaves;

    std::mutex phase0_mutex, phase1_mutex, phase2_mutex;
    std::condition_variable phase0_empty, phase0_full, phase1_empty, phase2_empty, phase1_full, phase2_full;
    FrameAnalysis *phase0_out, *phase1_out, *phase2_out;
    std::atomic< int > phase0_count, phase1_count, phase2_count, phase3_count;
    std::atomic< bool > exausted;

    // phase0 variables
    FrameWaiterContext settings_wait_ctx;

    // phase1 variables
    Phase1Context phase1_ctx;

    // phase3 variables
    int phase3_frame_num;
    std::set< std::pair< int, FrameAnalysis* > > reorder_buf;
    Phase3Context phase3_ctx;

    // working thread
    std::atomic< int > frame_num;
    std::mutex get_frame_mutex, settings_mutex;
    FrameSettings settings;
    FrameContext frame_ctx;
    FrameProducer *producer;
    std::mutex phase3_mutex;
    std::condition_variable phase3_empty, phase3_full;
    FrameAnalysis *phase3_out;
};

#endif // CONTEXT_H
