#ifndef CV_H
#define CV_H

#include <opencv2/core/core.hpp>

#include <string>

std::string getImgType(int imgTypeInt);
double evaluate_ECC_rho(cv::InputArray templateImage, cv::InputArray inputImage, cv::InputArray warpMatrix, int motionType, cv::InputArray inputMask);
std::vector<std::pair<cv::Point, float>> find_local_maxima(cv::Mat density, int x_rad, int y_rad, int max_count);

#endif // CV_H
