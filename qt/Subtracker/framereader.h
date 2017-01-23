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

class FrameClockTimePoint;

// Inspired to http://stackoverflow.com/a/17138183/807307
struct FrameClock {
    typedef std::chrono::duration< double > duration;
    typedef duration::rep rep;
    typedef duration::period period;
    typedef FrameClockTimePoint time_point;
    static const bool is_steady = false;

    static time_point now() noexcept;
};

struct FrameClockTimePoint : public std::chrono::time_point< FrameClock > {
    constexpr FrameClockTimePoint() :
        time_point< FrameClock >()
    {
    }
    constexpr FrameClockTimePoint(const FrameClock::duration &d) :
        time_point< FrameClock >(d)
    {
    }
    constexpr FrameClockTimePoint(const std::chrono::time_point< FrameClock > &x) :
        time_point< FrameClock >(x)
    {
    }
    std::chrono::system_clock::time_point to_system_clock() const;
    static FrameClockTimePoint from_system_clock(const std::chrono::system_clock::time_point &x);
    static FrameClockTimePoint from_double(double x);
};

struct FrameInfo {
  // The wall-clock time at which the frame was grabbed
  FrameClockTimePoint time;
  // The wall-clock time at which the frame is expected to be
  // processed by the program (only used internally by the queue
  // manager and only when performing live simulation)
  std::chrono::time_point< std::chrono::system_clock > playback_time;
  // Raw data before they are decoded to an OpenCV image
  std::shared_ptr< std::vector< char > > buffer;
  // The frame data itself
  cv::Mat data;
  // If false the frame could not be read (probably the video has
  // finished) and nothing else in this struct should be trusted
  bool valid;

  bool decode_buffer(tjhandle tj_dec);
};

struct JPEGFrameInfo : public FrameInfo {
public:
    bool decode_buffer();
private:
    tjhandle tj_dec;
};

class FrameProducer {
public:
  virtual FrameInfo get() = 0;
  virtual FrameInfo maybe_get() = 0;
  virtual FrameInfo get_last() = 0;
  virtual FrameInfo get_last_at_least_one() = 0;
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
    std::atomic<bool> finished;
    std::thread t;

  bool droppy;

  FrameCycle(bool droppy=false);
  void stats(const FrameInfo &info);
  void push(FrameInfo info);
  virtual bool init_thread();
  virtual bool process_frame() = 0;
  void terminate();
  void join_worker();

public:
  void start();
  void stop();
  bool is_finished();
  void set_droppy(bool droppy);
  int get_queue_length();
  void kill_queue();
  FrameInfo get();
  FrameInfo maybe_get();
  FrameInfo get_last();
  FrameInfo get_last_at_least_one();
  virtual ~FrameCycle();
};

class JPEGReader: public FrameCycle {
private:
  cv::Ptr< std::istream > fin;
  std::chrono::time_point< FrameClock > first_frame_time;
  bool first_frame_seen = false;
  bool from_file;

  int width;
  int height;

protected:
  bool process_frame();
  void open_file(std::string file_name);

public:
  JPEGReader(std::string file_name, bool from_file=false, bool simulate_live=false, int width=-1, int height=-1);
  ~JPEGReader();
  static void mangle_file_name(std::string &file_name, bool &from_file, bool &simulate_live);
};

#endif // FRAMEREADER_H
