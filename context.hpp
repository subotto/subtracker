#ifndef CONTEXT_HPP_
#define CONTEXT_HPP_

#include <opencv2/core/core.hpp>

using namespace cv;

struct FrameAnalysis {

  Mat frame;

};

struct SubtrackerContext {

  Mat ref_frame;
  Mat ref_mask;

  SubtrackerContext(Mat ref_frame, Mat ref_mask);

};

#endif
