
#include <iomanip>

#include "context.hpp"
#include "analysis.hpp"
#include "staging.hpp"
#include "subotto_tracking.hpp"

FrameSettings::FrameSettings(const Mat &ref_frame, const Mat &ref_mask)
  : table_frame_size_alpha(1.0), table_frame_size(reference.metrics.get_ideal_rectangle_size(table_frame_size_alpha)), local_maxima_limit(5), local_maxima_min_distance(0.10f) {

  this->reference.image = ref_frame;
  this->reference.mask = ref_mask;

}


FrameAnalysis::FrameAnalysis(const Mat &frame, int frame_num, const time_point< system_clock > &playback_time, const FrameSettings &frame_settings, control_panel_t &panel)
  : frame(frame), frame_num(frame_num), playback_time(playback_time), frame_settings(frame_settings), panel(panel), table_tracking_status(frame_settings.table_tracking_params, frame_settings.reference, frame_settings.table_frame_size), table_description(frame_settings.table_frame_size) {

  //logger(this->panel, "gio", DEBUG) << "table_frame_size: " << this->frame_settings.table_frame_size << endl;

}

void FrameAnalysis::setup_from_prev_table_tracking(const FrameAnalysis &prev_frame_analysis) {

  this->table_tracking_status = table_tracking_status_t(prev_frame_analysis.table_tracking_status);

}

void FrameAnalysis::setup_first_table_tracking() {

  this->table_tracking_status.detect_features();

}

void FrameAnalysis::track_table() {

  this->table_transform = ::track_table(this->frame, this->table_tracking_status, this->frame_settings.table_tracking_params, this->panel, this->frame_settings.reference, this->frame_settings.table_metrics, *this);

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

void FrameAnalysis::setup_first_table_analysis() {

  this->table_description.set_first_frame(this->table_frame);

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
                        this->bars_rot,
			this->foosmen_mask);

}

void FrameAnalysis::update_table_description() {

  ::do_update_table_description(this->panel, this->table_frame, this->table_analysis, this->table_description, this->foosmen_mask);

}

void FrameAnalysis::update_corrected_variance() {

  ::do_update_corrected_variance(this->panel, this->table_description);

}

void FrameAnalysis::search_spots() {

  ::search_spots(this->panel,
                 this->ball_density,
                 this->frame_settings.local_maxima_limit,
                 this->frame_settings.local_maxima_min_distance,
                 this->spots,
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
  circle(this->ball_display, ball_disp_pos, 8, Scalar(0.0, 1.0, 0.0), 2);

}

void FrameAnalysis::draw_foosmen_display() {

  this->table_frame.copyTo(this->foosmen_display);

	for(int side = 0; side < 2; side++) {
		for(int bar = 0; bar < BARS; bar++) {
			float xx = barx(side, bar, this->foosmen_display.size(), this->frame_settings.table_metrics, this->frame_settings.foosmen_metrics);
			line(this->foosmen_display, Point(xx, 0), Point(xx, this->foosmen_display.rows), Scalar(1.f, 1.f, 1.f));

			for(int i = 0; i < this->frame_settings.foosmen_metrics.count[bar]; i++) {
				float y = (0.5f + i - this->frame_settings.foosmen_metrics.count[bar] * 0.5f) * this->frame_settings.foosmen_metrics.distance[bar];
				float yy = (0.5f + (y + this->bars_shift[bar][side]) / this->frame_settings.table_metrics.width) * this->foosmen_display.rows;

				line(this->foosmen_display, Point(xx - 5, yy), Point(xx + 5, yy), Scalar(0.f, 1.f, 0.f));

        float len = 20;
        float r = this->bars_rot[bar][side];
        line(this->foosmen_display, Point(xx, yy), Point(xx + sin(r) * len, yy + cos(r) * len), Scalar(1.f, 1.f, 0.f), 1, 16);
			}
		}
	}

}

void FrameAnalysis::draw_foosmen_mask() {

  for(int side = 0; side < 2; side++) {
    for(int bar = 0; bar < BARS; bar++) {
      float xx = barx(side, bar, this->foosmen_mask.size(), this->frame_settings.table_metrics, this->frame_settings.foosmen_metrics);
      
      for(int i = 0; i < this->frame_settings.foosmen_metrics.count[bar]; i++) {
	float y = (0.5f + i - this->frame_settings.foosmen_metrics.count[bar] * 0.5f) * this->frame_settings.foosmen_metrics.distance[bar];
	float yy = (0.5f + (y + this->bars_shift[bar][side]) / this->frame_settings.table_metrics.width) * this->foosmen_mask.rows;
	
	//line(this->foosmen_mask, Point(xx - 5, yy), Point(xx + 5, yy), Scalar(0.f, 1.f, 0.f));
	rectangle(this->foosmen_mask, Point(xx - 20, yy - 15), Point(xx + 20, yy + 15), Scalar(.0f, .0f, .0f), CV_FILLED);
      }
    }
  }

}

double density_inf = -150.0;
double density_sup = 30.0;

static Mat normalize_matrix(const Mat &mat) {

  return (mat - density_inf) / (density_sup - density_inf);

}

void FrameAnalysis::show_all_displays() {

  show(this->panel, "frame", "frame", this->frame);
  show(this->panel, "frame", "table_frame", this->table_frame);
  show(this->panel, "frame", "mean", this->table_description.mean);

  //show(this->panel, "table detect", "reference image", this->frame_settings.reference.image);
  show(this->panel, "table detect", "reference image with keypoints", this->table_tracking_status.scaled_reference_with_keypoints);
  if (this->feature_matching_used) {
    show(this->panel, "table detect", "matches", this->detect_table_matches);
    show(this->panel, "table detect", "after feature matching", this->detect_table_after_matching);
  } else {
    show(this->panel, "table detect", "follow table before", this->follow_table_before);
  }

  show(this->panel, "ball tracking", "density", normalize_matrix(this->ball_density));
  show(this->panel, "ball tracking", "table NLL", normalize_matrix(this->table_analysis.nll));
  show(this->panel, "ball tracking", "ball LL", normalize_matrix(this->ball_analysis.ll));

  // Possibly draw foosmen display
  if (will_show(this->panel, "foosmen tracking", "foosmen")) {
    this->draw_foosmen_display();
    show(this->panel, "foosmen tracking", "foosmen", this->foosmen_display);
  }
  show(this->panel, "foosmen tracking", "foosmen mask", this->foosmen_mask);

  // Possibly draw ball display
  if (will_show(panel, "ball tracking", "ball")) {
    this->draw_ball_display();
    show(this->panel, "ball tracking", "ball", this->ball_display);
  }

  double density_min, density_max;
  minMaxLoc(this->ball_density, &density_min, &density_max, NULL, NULL);
  logger(this->panel, "ball tracking", DEBUG) << "Density minimum: " << density_min << "; density maximum: " << density_max << endl;

}


SubtrackerContext::SubtrackerContext(Mat ref_frame, Mat ref_mask, control_panel_t &panel, bool do_not_track_spots)
  : last_frame_num(0), frame_settings(ref_frame, ref_mask), panel(panel), spots_tracker(panel), spots_timeline_span(60), do_not_track_spots(do_not_track_spots) {

}

void SubtrackerContext::feed(const Mat &frame, time_point< system_clock > playback_time) {

  // Record playback time of first frame
  if (!this->first_frame_seen) {
    this->first_frame_seen = true;
    this->first_frame_playback_time = playback_time;
  }

  // Create new FrameAnalysis
  this->frame_analysis = new FrameAnalysis(frame, last_frame_num++, playback_time, this->frame_settings, this->panel);

  // Do the actual analysis
  this->do_table_tracking();
  this->do_analysis();
  this->do_spot_search();

  // Pass the results through the spots tracker
  this->do_spots_tracking();

  // Store FrameAnalysis for next round
  this->prev_frame_analysis = this->frame_analysis;

}

void SubtrackerContext::do_table_tracking() {

  if (this->prev_frame_analysis != NULL) {
    this->frame_analysis->setup_from_prev_table_tracking(*this->prev_frame_analysis);
  } else {
    this->frame_analysis->setup_first_table_tracking();
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
  } else {
    this->frame_analysis->setup_first_table_analysis();
  }

  // Do the actual analysis
  this->frame_analysis->analyze_table();
  this->frame_analysis->analyze_ball();
  this->frame_analysis->analyze_foosmen();

  // Update the running state
  this->frame_analysis->draw_foosmen_mask();
  this->frame_analysis->update_table_description();
  if (this->frame_analysis->frame_num % 5 == 0) {
    this->frame_analysis->update_corrected_variance();
  }

}

void SubtrackerContext::do_spot_search() {

  this->frame_analysis->search_spots();

}

void SubtrackerContext::do_spots_tracking() {

  if (this->do_not_track_spots) {
    this->ready_frames.push_back(*this->frame_analysis);
    return;
  }

  // Store the frame in our deque and in the SpotsTracker
  int frame_num = this->frame_analysis->frame_num;
  this->past_frames.push_back(*this->frame_analysis);
  this->spots_tracker.push_back(this->frame_analysis->spots, duration_cast< duration< double > >(this->frame_analysis->playback_time - this->first_frame_playback_time).count());

  // If we have already filled enough of the past...
  if (frame_num >= this->spots_timeline_span) {
    bool valid;
    Point2f position;
    tie(valid, position) = this->spots_tracker.front();
    int front_num = this->spots_tracker.get_front_num();
    this->spots_tracker.pop_front();

    FrameAnalysis frame_analysis = this->past_frames.front();
    this->past_frames.pop_front();
    assert(front_num == frame_analysis.frame_num);

    // Copy the position in the frame analysis if it is valid
    if (valid) {
      frame_analysis.ball_pos_x = position.x;
      frame_analysis.ball_pos_y = position.y;
      frame_analysis.ball_is_present = true;
    } else {
      frame_analysis.ball_is_present = false;
    }

    this->ready_frames.push_back(frame_analysis);
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
