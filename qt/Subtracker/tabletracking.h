#ifndef TABLETRACKING_H
#define TABLETRACKING_H

#include <tuple>
#include <vector>

#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>

std::tuple< std::vector< cv::KeyPoint >, cv::Mat > get_features(cv::Mat frame, cv::Mat mask, int features_per_level, int features_levels);

#endif // TABLETRACKING_H
