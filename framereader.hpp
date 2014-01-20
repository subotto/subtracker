#ifndef _FRAMEREADER_HPP
#define _FRAMEREADER_HPP
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/background_segm.hpp>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include "v4l2cap.hpp"

using namespace std;
using namespace chrono;
using namespace cv;

static const int frame_count_interval = 5;
static const int stats_interval = 1;
static const int buffer_size = 100;

struct frame_info {
    time_point<high_resolution_clock> timestamp;
    int number;
    Mat data;
};

class FrameReader {

private:
    deque<frame_info> queue;
    deque<time_point<high_resolution_clock>> frame_times;
    mutex queue_mutex;
    condition_variable queue_not_empty;
    atomic<bool> running;
    int count;
    thread t;
    VideoCapture cap;
    time_point<high_resolution_clock> last_stats;

public:
    FrameReader(int device) {
        running = true;
        count = 0;
        last_stats = high_resolution_clock::now();
        cap = v4l2cap(device, 320, 240, 125);
        t = thread(&FrameReader::read, this);
    }

    FrameReader(char* file) {
        running = true;
        count = 0;
        last_stats = high_resolution_clock::now();
        cap = VideoCapture(file);
        t = thread(&FrameReader::read, this);
    }

    void read() {
        // Delay di 1s per dare il tempo alla webcam di inizializzarsi
        this_thread::sleep_for(seconds(1));
        while(running) {
            Mat frame;
            if(!cap.read(frame)) {
                fprintf(stderr, "Error reading frame!\n");
                continue;
            }
            count++;
            auto now = high_resolution_clock::now();
            frame_times.push_back(now);
            while(now - frame_times.front() > seconds(frame_count_interval)) {
                frame_times.pop_front();
            }
            if(now - last_stats > seconds(stats_interval)) {
                fprintf(stderr, "Queue size: %lu\n", queue.size());
                fprintf(stderr,
                    "Number of frames in the last %d seconds: %lu\n",
                    frame_count_interval, frame_times.size());
                last_stats = now;
            }
            unique_lock<mutex> lock(queue_mutex);
            if (queue.size() < buffer_size) {
                queue.push_back({now, count, frame});
                queue_not_empty.notify_all();
            } else {
                fprintf(stderr, "Frame dropped!\n");
            }
        }
    }

    frame_info get() {
        unique_lock<mutex> lock(queue_mutex);
        while(queue.empty()) {
            queue_not_empty.wait(lock);
        }
        auto res = queue.front();
        queue.pop_front();
        return res;
    }

    ~FrameReader() {
        running = false;
        t.join();
    }
};

#endif
