
#include "context.hpp"

FrameSettings::FrameSettings(Mat ref_frame, Mat ref_mask)
  : table_frame_size(128, 64) {

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

  this->table_transform = ::track_table(this->frame, this->table_tracking_status, this->frame_settings.table_tracking_params, this->panel, this->frame_settings.reference);

}

void FrameAnalysis::warp_table_frame() {

  Mat frame32f;
  this->frame.convertTo(frame32f, CV_32F, 1 / 255.f);

	Mat warpTransform = this->table_transform * sizeToUnits(this->frame_settings.table_metrics, this->frame_settings.table_frame_size);
	warpPerspective(frame32f, this->table_frame, warpTransform, this->frame_settings.table_frame_size, CV_WARP_INVERSE_MAP | CV_INTER_LINEAR);

}


SubtrackerContext::SubtrackerContext(Mat ref_frame, Mat ref_mask, control_panel_t &panel)
  : frame_settings(ref_frame, ref_mask), prev_frame_analysis(NULL), panel(panel) {

}

void SubtrackerContext::feed(Mat frame, time_point< video_clock> timestamp) {

  // Create new FrameAnalysis
  this->frame_analysis = new FrameAnalysis(frame, timestamp, this->frame_settings, this->panel);

  // Do the actual analysis
  this->do_table_tracking();
  show(panel, "frame", "frame", this->frame_analysis->frame);
  show(panel, "frame", "table_frame", this->frame_analysis->table_frame);

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
  this->frame_analysis->warp_table_frame();

}
