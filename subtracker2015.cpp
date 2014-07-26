#include <iostream>

#include "control.hpp"
#include "framereader.hpp"
#include "context.hpp"

using namespace cv;
using namespace std;

static void feed_frames(FrameReader &frame_reader, SubtrackerContext &ctx, control_panel_t &panel) {

  for (int frame_num = 0; ; frame_num++) {

    // Read a new frame and perhaps terminate program
    auto frame_info = frame_reader.get();
    if (!frame_info.valid) break;

    ctx.feed(frame_info.data, frame_info.timestamp);

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

  SubtrackerContext ctx(ref_frame, ref_mask);

  // Initialize panel (GUI)
  init_control_panel(panel);

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

  feed_frames(*f, ctx, panel);

  // Graciously wait for the reader thread to stop
  delete f;

  return 0;

}
