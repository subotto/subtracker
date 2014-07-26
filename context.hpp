#ifndef CONTEXT_HPP_
#define CONTEXT_HPP_

#include <deque>

#include <opencv2/core/core.hpp>

#include "framereader_structs.hpp"

using namespace std;
using namespace cv;

class FrameSettings {

};

class FrameAnalysis {

private:

  // Global frame data
  Mat frame;
  time_point< video_clock > timestamp;
  FrameSettings frame_settings;

  // Reference frame data
  Mat ref_frame;
  Mat ref_mask;

  void copy_from_prev(const FrameAnalysis &prev_frame_analysis);

public:

  FrameAnalysis(Mat frame, time_point< video_clock > timestamp, FrameSettings frame_settings, Mat ref_frame, Mat ref_mask);
  FrameAnalysis(Mat frame, time_point< video_clock > timestamp, FrameSettings frame_settings, Mat ref_frame, Mat ref_mask, const FrameAnalysis &prev_frame_analysis);

};

class SubtrackerContext {

private:

  Mat ref_frame;
  Mat ref_mask;

  FrameSettings frame_settings;

  deque< FrameAnalysis > frames;

public:

  SubtrackerContext(Mat ref_frame, Mat ref_mask);

  void feed(Mat frame, time_point< video_clock > timestamp);

};

#endif
