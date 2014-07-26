#ifndef CONTEXT_HPP_
#define CONTEXT_HPP_

#include <deque>

#include <opencv2/core/core.hpp>

#include "control.hpp"
#include "framereader_structs.hpp"
#include "subotto_tracking.hpp"

using namespace std;
using namespace cv;

class FrameSettings {

public:

  table_tracking_params_t table_tracking_params;
  SubottoReference reference;
  Size table_frame_size;
  SubottoMetrics table_metrics;

  FrameSettings(Mat ref_frame, Mat ref_mask);

};

class FrameAnalysis {

public:

  // Global frame data
  Mat frame;
  time_point< video_clock > timestamp;
  FrameSettings frame_settings;
  control_panel_t &panel;

  // Table tracking
  table_tracking_status_t table_tracking_status;
  Mat table_transform;
  Mat table_frame;

  FrameAnalysis(Mat frame, time_point< video_clock > timestamp, FrameSettings frame_settings, control_panel_t &panel);

  void setup_from_prev_table_tracking(const FrameAnalysis &prev_frame_analysis);
  void track_table();
  void warp_table_frame();

};

class SubtrackerContext {

public:

  control_panel_t &panel;
  FrameSettings frame_settings;

  FrameAnalysis *frame_analysis;
  FrameAnalysis *prev_frame_analysis;

  SubtrackerContext(Mat ref_frame, Mat ref_mask, control_panel_t &panel);
  ~SubtrackerContext();

  void feed(Mat frame, time_point< video_clock > timestamp);
  void do_table_tracking();

};

#endif
