#ifndef CONTEXT_HPP_
#define CONTEXT_HPP_

#include <opencv2/core/core.hpp>

using namespace cv;

typedef struct {

  Mat ref_image;
  Mat ref_mask;

} SubtrackerContext;

#endif
