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
using namespace std;
using namespace chrono;
using namespace cv;

template<int buffer_size>
class FrameReader {

private:
    typedef pair<time_point<high_resolution_clock>, Mat> frame_type;
    deque<frame_type> queue;
    deque<time_point<high_resolution_clock>> frame_times;
    mutex queue_mutex;
    condition_variable queue_not_empty;
    atomic<bool> running;
    int count;
    thread t;
    VideoCapture cap;
    static const int frame_count_interval = 5;
    static const int stats_interval = 1;
    time_point<high_resolution_clock> last_stats;

public:
    FrameReader() {
        running = true;
        count = 0;
        t = thread(&FrameReader::read, this);
        last_stats = high_resolution_clock::now();
        cap = VideoCapture(0);
        if (!cap.isOpened()) {
            fprintf(stderr, "Error opening video capture 0\n");
            exit(1);
        }
    }

    void read() {
        // Delay di 1s per dare il tempo alla webcam di inizializzarsi
        this_thread::sleep_for(seconds(1));
        while(running) {
            count++;
            Mat frame;
            if(!cap.read(frame)) {
                fprintf(stderr, "Error reading frame!\n");
                continue;
            }
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
            if (queue.size() < buffer_size || count % 2) {
                queue.push_back(make_pair(now, frame));
                queue_not_empty.notify_all();
            } else {
                fprintf(stderr, "Frame dropped!\n");
            }
        }
    }

    frame_type get() {
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
