#include <iostream>

#include "control.hpp"
#include "framereader.hpp"
#include "context.hpp"

using namespace cv;
using namespace std;

static void key_pressed(SubtrackerContext &ctx, char c) {

  control_panel_t &panel = ctx.panel;

  switch(c) {

  case 'r':
    set_log_level(panel, "capture", VERBOSE);
    break;

  case 'p':
    toggle(panel, "control panel", TRACKBAR);
    break;

  case 'f':
    toggle(panel, "frame", SHOW);
    break;

  case 'c':
    toggle(panel, "cycle", TIME);
    break;

	case 's':
    set_log_level(panel, "table detect", VERBOSE);
    toggle(panel, "table detect", TRACKBAR, true);
    toggle(panel, "table detect", SHOW, true);
    break;

  case 'b':
    set_log_level(panel, "ball tracking", VERBOSE);
    toggle(panel, "ball tracking", TRACKBAR, true);
    toggle(panel, "ball tracking", SHOW, true);
    break;

  case 'm':
    toggle(panel, "foosmen tracking", TRACKBAR);
    toggle(panel, "foosmen tracking", SHOW);
    break;

  case 'l':
    toggle(panel, "foosmen metrics", TRACKBAR);
    toggle(panel, "reference metrics", TRACKBAR);
    break;

  case 'n':
    toggle(panel, "foosmen colors", TRACKBAR);
    break;

  case ' ':
    set_log_level(panel, "table detect", WARNING);
    set_log_level(panel, "ball tracking", WARNING);
    set_log_level(panel, "capture", WARNING);

    toggle(panel, "table detect", TRACKBAR, false);
    toggle(panel, "ball tracking", TRACKBAR, false);
    toggle(panel, "ball tracking", SHOW, false);

    toggle(panel, "foosmen tracking", TRACKBAR, false);
    toggle(panel, "foosmen tracking", SHOW, false);

    toggle(panel, "foosmen metrics", TRACKBAR, false);
    toggle(panel, "reference metrics", TRACKBAR, false);

    toggle(panel, "foosmen colors", TRACKBAR, false);

    toggle(panel, "frame", SHOW, false);
    toggle(panel, "cycle", TIME, false);
    toggle(panel, "control panel", TRACKBAR, false);
    break;
  }

}

int update_gui_skip = 1;
bool step_frame = false;
bool step_on_frame_produced = false;

static void feed_frames(FrameProducer &frame_producer, SubtrackerContext &ctx) {

  control_panel_t &panel = ctx.panel;

  trackbar(panel, "control panel", "update GUI skip", update_gui_skip, {0, 20, 1});

  //VideoWriter video_writer("ball_position.mpg", CV_FOURCC('P','I','M','1'), 100, ctx.frame_settings.table_frame_size, true);

  for (int frame_num = 0; ; frame_num++) {

    dump_time(panel, "cycle", "before GUI");

    // Every now and then, do GUI computation
    panel.update_display = (frame_num % (update_gui_skip + 1) == 0) || step_frame;
    if (panel.update_display) {

			namedWindow("control panel", WINDOW_NORMAL);

      // Check of key pressions (FIXME: this process just one key
      // pression every cycle)
    wait_other_key:
			int c = waitKey(1);
      if (c > 0) c &= 0xff;
      key_pressed(ctx, c);

      if (step_frame && c != '.') goto wait_other_key;

      dump_time(panel, "cycle", "wait key");

    }

    dump_time(panel, "cycle", "beginning real cycle");

    // Read a new frame and perhaps terminate program
    auto frame_info = frame_producer.get();
    if (!frame_info.valid) break;

    dump_time(panel, "cycle", "got a frame");

    // Feed the frame to the subtracker
    ctx.feed(frame_info.data, frame_info.playback_time);

    // Obtain as many processed frames as possible
    FrameAnalysis *frameAnalysis;
    while (frameAnalysis = ctx.get_processed_frame()) {
      // Print the frame
      cout << frameAnalysis->get_csv_line() << endl;

      // Show everything!
      frameAnalysis->show_all_displays();

      // And output the frame with the ball
      /*Mat video_frame;
      frameAnalysis->ball_display.convertTo(video_frame, CV_8U, 255.0);
      video_writer.write(video_frame);*/

      delete frameAnalysis;

      if (step_on_frame_produced) step_frame = true;
    }

    dump_time(panel, "cycle", "finished cycle");

  }

}

int main(int argc, char* argv[]) {

  // Parse arguments
  string videoName, referenceImageName, referenceImageMaskName;
  if (argc == 3 || argc == 4) {
    videoName = argv[1];
    referenceImageName = argv[2];

    if(argc == 4) {
      referenceImageMaskName = argv[3];
    }
  } else {
    cerr << "Usage: " << argv[0] << " <video> <reference subotto> [<reference subotto mask>]" << endl;
    cerr << "<video> can be: " << endl;
    cerr << "\tn (a single digit number) - live capture from video device n" << endl;
    cerr << "\tfilename+ (with a trailing plus) - simulate live capture from video file" << endl;
    cerr << "\tfilename (without a trailing plus) - batch analysis of video file" << endl;
    cerr << "<reference subotto> is the reference image used to look for the table" << endl;
    cerr << "<reference sumotto mask> is an optional B/W image, of the same size," << endl;
    cerr << "\twhere black spots indicate areas of the reference image to hide" << endl;
    cerr << "\twhen looking for the table (such as moving parts and spurious features)" << endl;
    return 1;
  }

  control_panel_t panel;

  // Read reference image and mask
  Mat ref_frame = imread(referenceImageName);
  Mat ref_mask;
  if(!referenceImageMaskName.empty()) {
    ref_mask = imread(referenceImageMaskName, CV_LOAD_IMAGE_GRAYSCALE);
  }

  SubtrackerContext ctx(ref_frame, ref_mask, panel);

  // Initialize panel (GUI)
  init_control_panel(panel);
  set_log_level(panel, "gio", DEBUG);

  // Open frame reader
  FrameProducer *f;
  if(videoName.size() == 1) {
    FrameReader *reader = new FrameReader(videoName[0] - '0', panel);
    reader->start();
    f = reader;
  } else {
    bool simulate_live = false;

    if(videoName.back() == '+') {
      videoName = videoName.substr(0, videoName.size()-1);
      simulate_live = true;
    }

    FrameReader *reader = new FrameReader(videoName.c_str(), panel, simulate_live);
    reader->start();
    f = reader;
  }

  feed_frames(*f, ctx);

  // Graciously wait for the reader thread to stop
  delete f;

  return 0;

}
