
#include <iomanip>

#include "context.hpp"
#include "analysis.hpp"
#include "staging.hpp"

FrameSettings::FrameSettings(Mat ref_frame, Mat ref_mask)
  : table_frame_size(128, 64) {

  this->reference.image = ref_frame;
  this->reference.mask = ref_mask;

}


FrameAnalysis::FrameAnalysis(Mat frame, int frame_num, time_point< video_clock > timestamp, time_point< system_clock > playback_time, FrameSettings frame_settings, control_panel_t &panel)
  : frame(frame), frame_num(frame_num), timestamp(timestamp), playback_time(playback_time), frame_settings(frame_settings), panel(panel), table_tracking_status(frame_settings.table_tracking_params, frame_settings.reference), table_description(frame_settings.table_frame_size) {

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

void FrameAnalysis::analyze_foosmen() {

  ::do_foosmen_analysis(this->panel,
                        this->foosmen_bars_metrics,
                        this->foosmen_bars_analysis,
                        this->frame_settings.table_metrics,
                        this->frame_settings.foosmen_metrics,
                        this->frame_settings.table_frame_size,
                        this->frame_settings.foosmen_params,
                        this->table_frame,
                        this->table_analysis,
                        this->bars_shift,
                        this->bars_rot);

}

void FrameAnalysis::update_table_description() {

  ::do_update_table_description(this->panel, this->table_frame, this->table_analysis, this->table_description);

}

void FrameAnalysis::update_corrected_variance() {

  ::do_update_corrected_variance(this->panel, this->table_description);

}

void FrameAnalysis::search_blobs() {

  ::search_blobs(this->panel,
                 this->ball_density,
                 this->frame_settings.local_maxima_limit,
                 this->frame_settings.local_maxima_min_distance,
                 this->blobs,
                 this->frame_settings.table_metrics,
                 this->frame_settings.table_frame_size,
                 this->frame_num);

}

string FrameAnalysis::get_csv_line() {

  stringstream buf;
  buf << setiosflags(ios::fixed) << setprecision(5);
  buf << duration_cast<duration<double>>(this->playback_time.time_since_epoch()).count();

  if (this->ball_is_present) {
    buf << "," << this->ball_pos_x << "," << this->ball_pos_y;
  } else {
    buf << ",,";
  }

  for(int side = 1; side >= 0; side--) {
    for(int bar = 0; bar < BARS; bar++) {
      buf << "," << -this->bars_shift[bar][side] << "," << this->bars_rot[bar][side];
    }
  }

  return buf.str();

}

void FrameAnalysis::draw_ball_display() {

  this->table_frame.copyTo(this->ball_display);
  Point2f ball_disp_pos;
  ball_disp_pos.x = (this->ball_pos_x / this->frame_settings.table_metrics.length + 0.5f) * this->ball_density.cols;
  ball_disp_pos.y = -(this->ball_pos_y / this->frame_settings.table_metrics.width - 0.5f) * this->ball_density.rows;
  circle(this->ball_display, ball_disp_pos, 8, Scalar(0,255,0), 2);

}

void FrameAnalysis::show_all_displays() {

  // Possibly draw ball display
  if (will_show(panel, "ball tracking", "ball")) {
    this->draw_ball_display();
    show(panel, "ball tracking", "ball", this->ball_display);
  }

}


SubtrackerContext::SubtrackerContext(Mat ref_frame, Mat ref_mask, control_panel_t &panel)
  : last_frame_num(0), frame_settings(ref_frame, ref_mask), prev_frame_analysis(NULL), panel(panel), blobs_tracker(panel) {

}

void SubtrackerContext::feed(Mat frame, time_point< video_clock> timestamp, time_point< system_clock > playback_time) {

  // Create new FrameAnalysis
  this->frame_analysis = new FrameAnalysis(frame, last_frame_num++, timestamp, playback_time, this->frame_settings, this->panel);

  // Do the actual analysis
  this->do_table_tracking();
  show(this->panel, "frame", "frame", this->frame_analysis->frame);
  show(this->panel, "frame", "table_frame", this->frame_analysis->table_frame);
  this->do_analysis();
  this->do_blob_search();

  // Pass the results through the blob tracker
  this->do_blobs_tracking();

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

  // Update running state from previous frame
  if (this->prev_frame_analysis != NULL) {
    this->frame_analysis->setup_from_prev_table_analysis(*this->prev_frame_analysis);
  }

  // Do the actual analysis
  this->frame_analysis->analyze_table();
  this->frame_analysis->analyze_ball();
  this->frame_analysis->analyze_foosmen();

  // Update the running state
  this->frame_analysis->update_table_description();
  if (this->frame_analysis->frame_num % 5 == 0) {
    this->frame_analysis->update_corrected_variance();
  }

}

void SubtrackerContext::do_blob_search() {

  this->frame_analysis->search_blobs();

}

void SubtrackerContext::do_blobs_tracking() {

  // Store the frame in our deque and in the BlobsTracker
  int frame_num = this->frame_analysis->frame_num;
  this->past_frames.push_back(*this->frame_analysis);
  this->blobs_tracker.InsertFrameInTimeline(this->frame_analysis->blobs, frame_num);

  // If we have already filled enough of the past...
  if (frame_num >= 2*this->blobs_timeline_span) {
    int initial_time = this->blobs_tracker.GetFrontTime();
    this->blobs_tracker.PopFrameFromTimeline();
    int processed_time = initial_time + this->blobs_timeline_span;

    if (initial_time % this->blobs_frames_to_process == 0) {
      logger(this->panel, "gio", DEBUG) << "Calling ProcessFrames(" << initial_time << ", " << processed_time << ", " << processed_time + this->blobs_frames_to_process << ") after processing frame " << frame_num << endl;
      vector< Point2f > positions = this->blobs_tracker.ProcessFrames(initial_time, processed_time, processed_time + this->blobs_frames_to_process);

      for (int i=0; i < positions.size(); i++) {
        FrameAnalysis frame_analysis = this->past_frames.front();
        this->past_frames.pop_front();
        logger(this->panel, "gio", DEBUG) << processed_time << " " << i << " " << frame_analysis.frame_num << endl;
        assert(processed_time + i == frame_analysis.frame_num);

        // Copy the position in the frame analysis if it is acceptable
        if (positions[i].x < 900.0 && positions[i].y < 900.0) {
          frame_analysis.ball_pos_x = positions[i].x;
          frame_analysis.ball_pos_y = positions[i].y;
          frame_analysis.ball_is_present = true;
        } else {
          frame_analysis.ball_is_present = false;
        }

        this->ready_frames.push_back(frame_analysis);
      }
    }
  }

  // Early frames will not be used in output, so we remove them from
  // the queue
  if (frame_num < this->blobs_timeline_span) {
    this->past_frames.pop_front();
  }

}

FrameAnalysis *SubtrackerContext::get_processed_frame() {

  if (this->ready_frames.empty()) {
    return NULL;
  } else {
    FrameAnalysis *ret = new FrameAnalysis(this->ready_frames.front());
    this->ready_frames.pop_front();
    return ret;
  }

}
