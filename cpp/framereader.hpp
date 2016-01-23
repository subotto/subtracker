#ifndef _FRAMEREADER_HPP
#define _FRAMEREADER_HPP

#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>

#include "control.hpp"
#include "framereader_structs.hpp"

using namespace std;
using namespace chrono;
using namespace cv;

static const seconds frame_count_interval(5);
static const int stats_interval = 1;
static const int buffer_size = 500;

class FrameProducer {
public:
  virtual FrameInfo get() = 0;
};

class FrameCycle : public FrameProducer {
private:
  void cycle();

protected:
	deque<FrameInfo> queue;
	mutex queue_mutex;
	condition_variable queue_not_empty;
	condition_variable queue_not_full;
	deque<time_point<video_clock>> frame_times;
	deque<time_point<video_clock>> frame_dropped;
	time_point<system_clock> last_stats;
	time_point<system_clock> video_start_time;

	control_panel_t& panel;

  bool can_drop_frames = false;
	bool rate_limited = false;

  long unsigned int enqueued_frames = 0;
	int count = 0;

	atomic<bool> running;
	thread t;

  FrameCycle(control_panel_t &panel);
  void stats(const FrameInfo &info);
  void push(FrameInfo info);
  virtual bool init_thread();
  virtual bool process_frame() = 0;

public:
  void start();
  FrameInfo get();
	~FrameCycle();
};

class FrameReader: public FrameCycle {
private:
	VideoCapture cap;
	bool fromFile = false;

protected:
  bool init_thread();
  bool process_frame();

public:
	FrameReader(int device, control_panel_t& panel, int width=320, int height=240, int fps=125);
	FrameReader(const char* file, control_panel_t& panel, bool simulate_live=false);
};

#endif
