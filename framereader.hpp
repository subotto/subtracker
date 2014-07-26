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

class FrameReader {

private:
	deque<FrameInfo> queue;
	deque<time_point<video_clock>> frame_times;
	deque<time_point<video_clock>> frame_dropped;
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
	bool can_drop_frames = false;

	control_panel_t& panel;

public:
	FrameReader(int device, control_panel_t& panel);
	FrameReader(const char* file, control_panel_t& panel, bool simulate_live = false);
	void read();
	FrameInfo get();
	~FrameReader();
};

#endif
