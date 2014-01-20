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

#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

using namespace std;
using namespace chrono;
using namespace cv;

static const int frame_count_interval = 5;
static const int stats_interval = 1;
static const int buffer_size = 100;

static int xioctl(int fh, int request, void *arg) {
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

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
    FrameReader() {
        running = true;
        count = 0;
        last_stats = high_resolution_clock::now();
        cap = VideoCapture(1);
        if (!cap.isOpened()) {
            fprintf(stderr, "Error opening video capture 0\n");
            exit(1);
        }


        cap.set(CV_CAP_PROP_FRAME_WIDTH, 320);
        cap.set(CV_CAP_PROP_FRAME_HEIGHT, 240);

        // Ottengo il file descriptor della webcam
        int fd = -1;
        DIR* dir;
        struct dirent *ent;
        // Possiamo assumere che questo ci sia...
        dir = opendir("/proc/self/fd");
        while ((ent=readdir(dir)) != NULL) {
            // Salta . e ..
            if(ent->d_name[0] == '.') continue;
            char fl[100] = "/proc/self/fd/";
            char dst[100];
            strcat(fl, ent->d_name);
            if(readlink(fl, dst, 100) == -1) {
                fprintf(stderr, "Error listing file descriptors links\n");
                exit(2);
            }
            if(strstr(dst, "/dev/video")) {
                fd = atoi(ent->d_name);
            }
        }
        if(fd == -1) {
            fprintf(stderr, "Cannot find webcam's file descriptor "
                            "- this should not happen!\n");
            exit(3);
        }
        closedir(dir);

        v4l2_streamparm streamparm;
        streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(xioctl(fd, VIDIOC_G_PARM, &streamparm) < 0) {
            fprintf(stderr, "Error getting stream parameters!\n");
            exit(4);
        }
        if(!(streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
            fprintf(stderr, "Device does not support setting frame rate!");
            exit(5);
        }
        // 125 fps
        streamparm.parm.capture.timeperframe.numerator = 1;
        streamparm.parm.capture.timeperframe.denominator = 125;
        if(xioctl(fd, VIDIOC_S_PARM, &streamparm) < 0) {
            fprintf(stderr, "Error setting device framerate!\n");
        }

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
