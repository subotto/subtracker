
#include "context.hpp"
#include "analysis.hpp"

FrameSettings::FrameSettings(Mat ref_frame, Mat ref_mask)
  : table_frame_size(128, 64) {

  this->reference.image = ref_frame;
  this->reference.mask = ref_mask;

}


FrameAnalysis::FrameAnalysis(Mat frame, int frame_num, time_point< video_clock > timestamp, FrameSettings frame_settings, control_panel_t &panel, Size table_frame_size)
  : frame(frame), frame_num(frame_num), timestamp(timestamp), frame_settings(frame_settings), panel(panel), table_tracking_status(frame_settings.table_tracking_params, frame_settings.reference), table_description(table_frame_size) {

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

void FrameAnalysis::setup_from_prev_table_analysis(const FrameAnalysis & prev_frame_analysis) {

  this->table_description = prev_frame_analysis.table_description;

}

void FrameAnalysis::analyze_table() {

  ::do_table_analysis(this->panel, this->table_frame, this->table_description, this->table_analysis);

}

void FrameAnalysis::analyze_ball() {

  ::do_ball_analysis(this->panel, this->table_frame, this->ball_description, this->table_analysis, this->ball_analysis, this->ball_density);

}

void FrameAnalysis::update_table_description() {

  ::do_update_table_description(this->panel, this->table_frame, this->table_analysis, this->table_description);

}

void FrameAnalysis::update_corrected_variance() {

  ::do_update_corrected_variance(this->panel, this->table_description);

}


SubtrackerContext::SubtrackerContext(Mat ref_frame, Mat ref_mask, control_panel_t &panel)
  : last_frame_num(0), frame_settings(ref_frame, ref_mask), prev_frame_analysis(NULL), panel(panel) {

}

void SubtrackerContext::feed(Mat frame, time_point< video_clock> timestamp) {

  // Create new FrameAnalysis
  Size table_frame_size(128, 64);
  this->frame_analysis = new FrameAnalysis(frame, last_frame_num++, timestamp, this->frame_settings, this->panel, table_frame_size);

  // Do the actual analysis
  this->do_table_tracking();
  show(this->panel, "frame", "frame", this->frame_analysis->frame);
  show(this->panel, "frame", "table_frame", this->frame_analysis->table_frame);
  this->do_analysis();

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
  dump_time(this->panel, "cycle", "track table");
  this->frame_analysis->warp_table_frame();
  dump_time(this->panel, "cycle", "warp table frame");

}

void SubtrackerContext::do_analysis() {

  if (this->prev_frame_analysis != NULL) {
    this->frame_analysis->setup_from_prev_table_analysis(*this->prev_frame_analysis);
  }
  this->frame_analysis->analyze_table();
  this->frame_analysis->analyze_ball();
  this->frame_analysis->update_table_description();
  if (this->frame_analysis->frame_num % 5 == 0) {
    this->frame_analysis->update_corrected_variance();
  }

}
