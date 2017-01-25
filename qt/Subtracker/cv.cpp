
#include "cv.h"

#include <opencv2/video/video.hpp>

#include <string>

using namespace std;
using namespace cv;

// Taken from http://stackoverflow.com/a/12336381/807307
// take number image type number (from cv::Mat.type()), get OpenCV's enum string.
string getImgType(int imgTypeInt)
{
    int numImgTypes = 35; // 7 base types, with five channel options each (none or C1, ..., C4)

    int enum_ints[] =       {CV_8U,  CV_8UC1,  CV_8UC2,  CV_8UC3,  CV_8UC4,
                             CV_8S,  CV_8SC1,  CV_8SC2,  CV_8SC3,  CV_8SC4,
                             CV_16U, CV_16UC1, CV_16UC2, CV_16UC3, CV_16UC4,
                             CV_16S, CV_16SC1, CV_16SC2, CV_16SC3, CV_16SC4,
                             CV_32S, CV_32SC1, CV_32SC2, CV_32SC3, CV_32SC4,
                             CV_32F, CV_32FC1, CV_32FC2, CV_32FC3, CV_32FC4,
                             CV_64F, CV_64FC1, CV_64FC2, CV_64FC3, CV_64FC4};

    string enum_strings[] = {"CV_8U",  "CV_8UC1",  "CV_8UC2",  "CV_8UC3",  "CV_8UC4",
                             "CV_8S",  "CV_8SC1",  "CV_8SC2",  "CV_8SC3",  "CV_8SC4",
                             "CV_16U", "CV_16UC1", "CV_16UC2", "CV_16UC3", "CV_16UC4",
                             "CV_16S", "CV_16SC1", "CV_16SC2", "CV_16SC3", "CV_16SC4",
                             "CV_32S", "CV_32SC1", "CV_32SC2", "CV_32SC3", "CV_32SC4",
                             "CV_32F", "CV_32FC1", "CV_32FC2", "CV_32FC3", "CV_32FC4",
                             "CV_64F", "CV_64FC1", "CV_64FC2", "CV_64FC3", "CV_64FC4"};

    for(int i=0; i<numImgTypes; i++)
    {
        if(imgTypeInt == enum_ints[i]) return enum_strings[i];
    }
    return "unknown image type";
}

// Taken and adapted from https://github.com/opencv/opencv/blob/dd8589c3523ade76ddb887c92ebbec78b17d70fe/modules/video/src/ecc.cpp
double evaluate_ECC_rho(InputArray templateImage, InputArray inputImage, InputArray warpMatrix, int motionType, InputArray inputMask) {

    Mat src = templateImage.getMat();//template iamge
    Mat dst = inputImage.getMat(); //input image (to be warped)
    Mat map = warpMatrix.getMat(); //warp (transformation)

    CV_Assert(!src.empty());
    CV_Assert(!dst.empty());


    if( ! (src.type()==dst.type()))
        CV_Error( Error::StsUnmatchedFormats, "Both input images must have the same data type" );

    //accept only 1-channel images
    if( src.type() != CV_8UC1 && src.type()!= CV_32FC1)
        CV_Error( Error::StsUnsupportedFormat, "Images must have 8uC1 or 32fC1 type");

    if( map.type() != CV_32FC1)
        CV_Error( Error::StsUnsupportedFormat, "warpMatrix must be single-channel floating-point matrix");

    CV_Assert (map.cols == 3);
    CV_Assert (map.rows == 2 || map.rows ==3);

    CV_Assert (motionType == MOTION_AFFINE || motionType == MOTION_HOMOGRAPHY ||
        motionType == MOTION_EUCLIDEAN || motionType == MOTION_TRANSLATION);

    if (motionType == MOTION_HOMOGRAPHY){
        CV_Assert (map.rows ==3);
    }

    const int ws = src.cols;
    const int hs = src.rows;
    const int wd = dst.cols;
    const int hd = dst.rows;

    Mat templateZM    = Mat(hs, ws, CV_32F);// to store the (smoothed)zero-mean version of template
    Mat templateFloat = Mat(hs, ws, CV_32F);// to store the (smoothed) template
    Mat imageFloat    = Mat(hd, wd, CV_32F);// to store the (smoothed) input image
    Mat imageWarped   = Mat(hs, ws, CV_32F);// to store the warped zero-mean input image
    Mat imageMask		= Mat(hs, ws, CV_8U); //to store the final mask

    //to use it for mask warping
    Mat preMask;
    if(inputMask.empty())
        preMask = Mat::ones(hd, wd, CV_8U);
    else
        threshold(inputMask, preMask, 0, 1, THRESH_BINARY);

    //gaussian filtering is optional
    src.convertTo(templateFloat, templateFloat.type());
    GaussianBlur(templateFloat, templateFloat, Size(5, 5), 0, 0);

    Mat preMaskFloat;
    preMask.convertTo(preMaskFloat, CV_32F);
    GaussianBlur(preMaskFloat, preMaskFloat, Size(5, 5), 0, 0);
    // Change threshold.
    preMaskFloat *= (0.5/0.95);
    // Rounding conversion.
    preMaskFloat.convertTo(preMask, preMask.type());
    preMask.convertTo(preMaskFloat, preMaskFloat.type());

    dst.convertTo(imageFloat, imageFloat.type());
    GaussianBlur(imageFloat, imageFloat, Size(5, 5), 0, 0);

    const int imageFlags = INTER_LINEAR  + WARP_INVERSE_MAP;
    const int maskFlags  = INTER_NEAREST + WARP_INVERSE_MAP;

    if (motionType != MOTION_HOMOGRAPHY)
    {
        warpAffine(imageFloat, imageWarped,     map, imageWarped.size(),     imageFlags);
        warpAffine(preMask,    imageMask,       map, imageMask.size(),       maskFlags);
    }
    else
    {
        warpPerspective(imageFloat, imageWarped,     map, imageWarped.size(),     imageFlags);
        warpPerspective(preMask,    imageMask,       map, imageMask.size(),       maskFlags);
    }

    Scalar imgMean, imgStd, tmpMean, tmpStd;
    meanStdDev(imageWarped,   imgMean, imgStd, imageMask);
    meanStdDev(templateFloat, tmpMean, tmpStd, imageMask);

    subtract(imageWarped,   imgMean, imageWarped, imageMask);//zero-mean input
    templateZM = Mat::zeros(templateZM.rows, templateZM.cols, templateZM.type());
    subtract(templateFloat, tmpMean, templateZM,  imageMask);//zero-mean template

    const double tmpNorm = std::sqrt(countNonZero(imageMask)*(tmpStd.val[0])*(tmpStd.val[0]));
    const double imgNorm = std::sqrt(countNonZero(imageMask)*(imgStd.val[0])*(imgStd.val[0]));
    const double correlation = templateZM.dot(imageWarped);
    double rho = correlation/(imgNorm*tmpNorm);

    return rho;
}

vector<pair<Point, float>> find_local_maxima(Mat density, int x_rad, int y_rad, int max_count) {
    Mat dilatedDensity;
    dilate(density, dilatedDensity, Mat::ones(2 * y_rad + 1, 2 * x_rad + 1, CV_8U));

    Mat localMaxMask = (density >= dilatedDensity);

    Mat_<Point> nonZero;
    findNonZero(localMaxMask, nonZero);

    vector<pair<Point, float>> localMaxima;
    for(int i = 0; i < nonZero.rows; i++) {
        Point p = *nonZero[i];
        float w = density.at<float>(p);

        localMaxima.push_back(make_pair(p, w));
    }

    int count = min(localMaxima.size(), size_t(max_count));
    nth_element(localMaxima.begin(), localMaxima.begin() + count, localMaxima.end(), [](const auto &a, const auto &b) {
        return a.second > b.second;
    });
    localMaxima.resize(count);

    return localMaxima;
}
