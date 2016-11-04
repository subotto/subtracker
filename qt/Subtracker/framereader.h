#ifndef FRAMEREADER_H
#define FRAMEREADER_H

#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <string>
#include <fstream>

#include <opencv2/core/core.hpp>
#include <turbojpeg.h>

struct FrameInfo {
  // The wall-clock time at which the frame was grabbed
  std::chrono::time_point< std::chrono::system_clock > time;
  // The wall-clock time at which the frame is expected to be
  // processed by the program (only used internally by the queue
  // manager and only when performing live simulation)
    std::chrono::time_point< std::chrono::system_clock > playback_time;
  // The frame data itself
    cv::Mat data;
  // If false the frame could not be read (probably the video has
  // finished) and nothing else in this struct should be trusted
  bool valid;
};

class FrameProducer {
public:
  virtual FrameInfo get() = 0;
};

static const std::chrono::seconds frame_count_interval(5);
static const std::chrono::seconds stats_interval(1);
static const int buffer_size = 500;

class FrameCycle : public FrameProducer {
private:
  void cycle();
  void process_stats();

protected:
    std::deque<FrameInfo> queue;
    std::mutex queue_mutex;
    std::condition_variable queue_not_empty;
    std::condition_variable queue_not_full;
    std::deque<std::chrono::time_point<std::chrono::system_clock>> frame_times;
    std::deque<std::chrono::time_point<std::chrono::system_clock>> frame_dropped;
    std::chrono::time_point<std::chrono::system_clock> last_stats;
    std::chrono::time_point<std::chrono::system_clock> video_start_playback_time;

  bool can_drop_frames = true;
    bool rate_limited = false;

  long unsigned int enqueued_frames = 0;
    int count = 0;

    std::atomic<bool> running;
    std::thread t;

  bool droppy;

  FrameCycle(bool droppy=false);
  void stats(const FrameInfo &info);
  void push(FrameInfo info);
  virtual bool init_thread();
  virtual bool process_frame() = 0;
  void terminate();

public:
  void start();
  void set_droppy(bool droppy);
  FrameInfo get();
    ~FrameCycle();
};

class JPEGReader: public FrameCycle {
private:
  cv::Ptr< std::istream > fin;
  tjhandle tj_dec;
  std::chrono::time_point<std::chrono::system_clock> first_frame_time;
  bool first_frame_seen = false;
  bool from_file;

  int width;
  int height;

protected:
  bool process_frame();
  void open_file(std::string file_name);

public:
  JPEGReader(std::string file_name, bool from_file, bool simulate_live, int width=-1, int height=-1);
  ~JPEGReader();
  static void mangle_file_name(std::string &file_name, bool &from_file, bool &simulate_live);
};

#endif // FRAMEREADER_H
