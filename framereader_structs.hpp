#ifndef _FRAMEREADER_STRUCTS_HPP
#define _FRAMEREADER_STRUCTS_HPP

#include <chrono>

using namespace std;
using namespace chrono;
using namespace cv;

// video clock - starting at first frame
struct video_clock {
	typedef nanoseconds duration;
};

struct FrameInfo {
  // The time position in the stream
	time_point<video_clock> timestamp;
  // The wall-clock time at which the frame is expected to be shown
	time_point<system_clock> playback_time;
  // The frame data itself
	Mat data;
  // If false the frame could not be read (probably the video has
  // finished) and nothing else in this struct should be trusted
  bool valid;
};

#endif
