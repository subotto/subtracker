#ifndef _FRAMEREADER_STRUCTS_HPP
#define _FRAMEREADER_STRUCTS_HPP

#include <chrono>

using namespace std;
using namespace chrono;
using namespace cv;

struct FrameInfo {
  // The wall-clock time at which the frame was grabbed
  time_point< system_clock > time;
  // The wall-clock time at which the frame is expected to be
  // processed by the program (only used internally by the queue
  // manager and only when performing live simulation)
	time_point< system_clock > playback_time;
  // The frame data itself
	Mat data;
  // If false the frame could not be read (probably the video has
  // finished) and nothing else in this struct should be trusted
  bool valid;
};

#endif
