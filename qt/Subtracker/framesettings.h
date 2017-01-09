#ifndef FRAMESETTINGS_H
#define FRAMESETTINGS_H

#include <opencv2/core/core.hpp>

struct FrameSettings {
    // The four corners of the table, in this order: red defence, red attack, blue defence, blue attack
    cv::Point2f table_corners[4] = { { 100.0, 400.0 }, { 600.0, 400.0 }, { 600.0, 100.0 }, { 100.0, 100.0 } };
    cv::Size intermediate_size = { 400, 300 };
    // OpenCV uses BGR colors; indexes are: 0 -> red foosmen, 1 -> blue foosmen, 2 -> ball
    cv::Scalar objects_colors[3] = { { 0.0f, 0.0f, 1.0f}, { 1.0f, 0.0f, 0.0f }, { 0.85f, 0.85f, 0.85f } };
    float objects_color_stddev[3] = { 0.15f, 0.15f, 0.075f };

    cv::Mat ref_image;
    std::string ref_image_filename;
    cv::Mat ref_mask;
    std::string ref_mask_filename;

    int feats_hessian_threshold = 600;
    int feats_n_octaves = 3;

    // Actual size in meters
    float table_length = 1.15;
    float table_width = 0.70;
};

struct FrameCommands {
    bool retrack_table = false;
    bool regen_feature_detector = false;
    bool new_ref = false;
    bool new_mask = false;
};

#endif // FRAMESETTINGS_H
