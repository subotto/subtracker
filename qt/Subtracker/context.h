#ifndef CONTEXT_H
#define CONTEXT_H

#include "framesettings.h"
#include "frameanalysis.h"
#include "framereader.h"

#include <chrono>
#include <vector>
#include <mutex>
#include <condition_variable>

#include <opencv2/core/core.hpp>

class Context
{
public:
    Context(int slave_num);
    void feed(const cv::Mat &frame, std::chrono::time_point< std::chrono::system_clock > playback_time);
    ~Context();

private:
    void slave_fn(int id);
    FrameSettings settings;
    std::vector< std::thread > slaves;
    std::mutex slave_mutex;
    std::condition_variable slave_cond;
    std::vector< FrameAnalysis* > slaves_output;
    FrameAnalysis *slaves_input;
};

#endif // CONTEXT_H
