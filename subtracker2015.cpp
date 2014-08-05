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

int update_gui_skip = 5;

static void feed_frames(FrameReader &frame_reader, SubtrackerContext &ctx) {

  control_panel_t &panel = ctx.panel;

  trackbar(panel, "control panel", "update GUI skip", update_gui_skip, {0, 20, 1});

  for (int frame_num = 0; ; frame_num++) {

    dump_time(panel, "cycle", "feed new frame");

    // Every now and then, do GUI computation
    panel.update_display = (frame_num % (update_gui_skip + 1) == 0);
    if (panel.update_display) {

			namedWindow("control panel", WINDOW_NORMAL);

      // Check of key pressions (FIXME: this process just one key
      // pression every cycle)
			int c = waitKey(1);
      if (c > 0) c &= 0xff;
      key_pressed(ctx, c);

      dump_time(panel, "cycle", "wait key");

    }

    // Read a new frame and perhaps terminate program
    auto frame_info = frame_reader.get();
    if (!frame_info.valid) break;

    dump_time(panel, "cycle", "got a frame");

    // Feed the frame to the subtracker
    ctx.feed(frame_info.data, frame_info.timestamp, frame_info.playback_time);

    dump_time(panel, "cycle", "finished frame feed");

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
  set_log_level(panel, "gio", VERBOSE);

  // Open frame reader
  FrameReader *f;
  if(videoName.size() == 1) {
    f = new FrameReader(videoName[0] - '0', panel);
  } else {
    bool simulate_live = false;

    if(videoName.back() == '+') {
      videoName = videoName.substr(0, videoName.size()-1);
      simulate_live = true;
    }

    f = new FrameReader(videoName.c_str(), panel, simulate_live);
  }

  feed_frames(*f, ctx);

  // Graciously wait for the reader thread to stop
  delete f;

  return 0;

}
