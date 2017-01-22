#ifndef CV_H
#define CV_H

#include <opencv2/core/core.hpp>

#include <string>

std::string getImgType(int imgTypeInt);
double evaluate_ECC_rho(cv::InputArray templateImage, cv::InputArray inputImage, cv::InputArray warpMatrix, int motionType, cv::InputArray inputMask);

#endif // CV_H
