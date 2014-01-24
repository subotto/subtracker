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

static const seconds frame_count_interval(5);
static const int stats_interval = 1;
static const int buffer_size = 100;

// video clock - starting at first frame
struct video_clock {
	typedef nanoseconds duration;
};

struct frame_info {
    time_point<video_clock> timestamp;
    time_point<system_clock> playback_time;
    Mat data;
};

class FrameReader {

private:
    deque<frame_info> queue;
    deque<time_point<video_clock>> frame_times;
    mutex queue_mutex;
    condition_variable queue_not_empty;
    condition_variable queue_not_full;
    atomic<bool> running;
    int count;
    thread t;
    VideoCapture cap;
    time_point<system_clock> last_stats;
    time_point<system_clock> video_start_time;

    bool fromFile = false;
    bool rate_limited = false;

public:
    FrameReader(int device) {
        running = true;
        count = 0;
        last_stats = high_resolution_clock::now();
        cap = v4l2cap(device, 320, 240, 125);
        t = thread(&FrameReader::read, this);
        video_start_time = system_clock::now();
    }

    FrameReader(const char* file, bool rate_limited = true) {
        running = true;
        count = 0;
        last_stats = high_resolution_clock::now();
        cap = VideoCapture(file);
        if(!cap.isOpened()) {
            fprintf(stderr, "Cannot open video file!\n");
            exit(1);
        }
        t = thread(&FrameReader::read, this);
        fromFile = true;
        this->rate_limited = rate_limited;
    }

    void read() {
        if(!fromFile) {
            // Delay di 1s per dare il tempo alla webcam di inizializzarsi
            this_thread::sleep_for(seconds(1));
        }
        video_start_time = system_clock::now();
        long unsigned int enqueued_frames = 0;
        while(running) {
            Mat frame;
            if(!cap.read(frame)) {
                if(fromFile) {
                    fprintf(stderr, "Video ended.\n");
                    exit(0);
                } else {
                    fprintf(stderr, "Error reading frame!\n");
                    continue;
                }
            }
            count++;
            auto now = system_clock::now();

            time_point<video_clock> timestamp;
            if(fromFile) {
            	double posMsec = cap.get(CV_CAP_PROP_POS_MSEC);
            	timestamp = time_point<video_clock>(duration_cast<nanoseconds>(duration<double, milli>(posMsec)));
            } else {
            	timestamp = time_point<video_clock>(now - video_start_time);
            }

            frame_times.push_back(timestamp);
            while(timestamp - frame_times.front() > frame_count_interval) {
                frame_times.pop_front();
            }

            if(now - last_stats > seconds(stats_interval)) {
                fprintf(stderr, "Queue size: %lu\n",
                    (long unsigned)queue.size());
                fprintf(stderr, "Received %lu frames in the last %lu seconds.\n",
                    (long unsigned) frame_times.size(),
                    (long unsigned) seconds(frame_count_interval).count());
                fprintf(stderr, "Processed %lu frames in %.3f seconds\n",
                    (long unsigned) (enqueued_frames - queue.size()),
                    float(duration_cast<duration<float>>(now - video_start_time).count()));
                last_stats = now;
            }
            unique_lock<mutex> lock(queue_mutex);
            if (fromFile) {
                while (queue.size() >= buffer_size) {
                    queue_not_full.wait(lock);
                }
            }

            assert(!fromFile || queue.size() < buffer_size);

			auto playback_time = video_start_time + timestamp.time_since_epoch();

			if(rate_limited) {
				this_thread::sleep_until(playback_time);
			}

			if (queue.size() < buffer_size) {
				queue.push_back( { timestamp, playback_time, frame });
				queue_not_empty.notify_all();
				enqueued_frames++;
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
        queue_not_full.notify_all();
        return res;
    }

    ~FrameReader() {
        running = false;
        t.join();
    }
};

#endif
