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
    Context(size_t slave_num, FrameProducer *producer, const FrameSettings &settings);
    FrameAnalysis *get();
    void wait();
    FrameAnalysis *maybe_get();
    ~Context();
    void set_settings(const FrameSettings &settings);
    bool is_finished();

private:
    void working_thread();
    template< class Function, class... Args >
    void create_thread(std::string, Function &&f, Args &&... args);

    std::atomic< bool > running;
    std::vector< std::thread > slaves;

    // working thread
    std::atomic< int > frame_num;
    std::mutex get_frame_mutex, settings_mutex;
    FrameSettings settings;
    FrameContext frame_ctx;
    FrameProducer *producer;
    std::mutex output_mutex;
    std::condition_variable output_empty, output_full;
    FrameAnalysis *output;
    FrameWaiterContext output_waiter;
};

#endif // CONTEXT_H
