
#include "tabletracking.h"

#include <opencv2/xfeatures2d.hpp>

using namespace std;
using namespace cv;

tuple< vector< KeyPoint >, Mat > get_features(Mat frame, Mat mask, int features_per_level, int features_levels) {

  auto gftd = GFTTDetector::create(features_per_level);
  // PyramidAdaptedFeatureDetector does not exist anymore in OpenCV 3,
  // so for workarounds:
  // http://answers.opencv.org/question/85741/is-pyramidadaptedfeaturedetector-gone-in-opencv-3/
  //PyramidAdaptedFeatureDetector fd(gftd, features_levels);
  auto &fd = gftd;
  vector< KeyPoint > features;
  fd->detect(frame, features, mask);

  auto bde = xfeatures2d::BriefDescriptorExtractor::create(64);
  // As above
    //OpponentColorDescriptorExtractor de(bde);
  auto &de = bde;
  Mat features_descriptions;
  de->compute(frame, features, features_descriptions);

  return make_tuple(features, features_descriptions);

}
