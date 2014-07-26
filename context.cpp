
#include "context.hpp"

FrameSettings::FrameSettings(Mat ref_frame, Mat ref_mask) {

  this->reference.image = ref_frame;
  this->reference.mask = ref_mask;

}

FrameAnalysis::FrameAnalysis(Mat frame, time_point< video_clock > timestamp, FrameSettings frame_settings, control_panel_t &panel)
  : frame(frame), timestamp(timestamp), frame_settings(frame_settings), panel(panel), table_tracking_status(frame_settings.table_tracking_params, frame_settings.reference) {

}

void FrameAnalysis::setup_from_prev_table_tracking(const FrameAnalysis &prev_frame_analysis) {

  this->table_tracking_status = prev_frame_analysis.table_tracking_status;

}

void FrameAnalysis::track_table() {

  Mat transform = ::track_table(this->frame, this->table_tracking_status, this->frame_settings.table_tracking_params, this->panel, this->frame_settings.reference);

}

SubtrackerContext::SubtrackerContext(Mat ref_frame, Mat ref_mask, control_panel_t &panel)
  : frame_settings(ref_frame, ref_mask), prev_frame_analysis(NULL), panel(panel) {

}

void SubtrackerContext::feed(Mat frame, time_point< video_clock> timestamp) {

  // Create new FrameAnalysis
  this->frame_analysis = new FrameAnalysis(frame, timestamp, this->frame_settings, this->panel);

  // Do the actual analysis
  this->do_table_tracking();

  // Store FrameAnalysis for next round
  if (this->prev_frame_analysis != NULL) {
    delete this->prev_frame_analysis;
  }
  this->prev_frame_analysis = this->frame_analysis;

}

SubtrackerContext::~SubtrackerContext() {

  if (this->prev_frame_analysis != NULL) {
    delete this->prev_frame_analysis;
  }

}

void SubtrackerContext::do_table_tracking() {

  if (this->prev_frame_analysis != NULL) {
    this->frame_analysis->setup_from_prev_table_tracking(*this->prev_frame_analysis);
  }
  this->frame_analysis->track_table();

}
