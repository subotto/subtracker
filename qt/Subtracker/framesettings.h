#ifndef FRAMESETTINGS_H
#define FRAMESETTINGS_H

#include <opencv2/core/core.hpp>

struct FrameSettings {
    // The four corners of the table, in this order: red defence, red attack, blue defence, blue attack
    //cv::Point2f table_corners[4] = { { 100.0, 400.0 }, { 600.0, 400.0 }, { 600.0, 100.0 }, { 100.0, 100.0 } };
    std::vector< cv::Point2f > ref_corners = {{ 119.0, 382.0 }, { 547.0, 379.0 }, { 550.0, 122.0 }, { 116.0, 124.0 }};
    float intermediate_alpha = 1.0;
    //cv::Size intermediate_size = { 400, 300 };
    // OpenCV uses BGR colors; indexes are: 0 -> red foosmen, 1 -> blue foosmen, 2 -> ball
    cv::Scalar objects_colors[3] = { { 0.0f, 0.0f, 1.0f}, { 1.0f, 0.0f, 0.0f }, { 0.85f, 0.85f, 0.85f } };
    float objects_color_stddev[3] = { 0.15f, 0.15f, 0.075f };

    cv::Mat ref_image;
    std::string ref_image_filename;
    cv::Mat ref_mask;
    std::string ref_mask_filename;

    // SURF
    int feats_hessian_threshold = 600;
    int feats_n_octaves = 10;
    int ransac_threshold = 20;
    float det_threshold = 0.1;

    // Optical flow
    int gftt_max_corners = 30;
    double gftt_quality_level = 0.01;
    double gftt_min_distance = 3.0;
    int gftt_blockSize = 3;
    bool gftt_use_harris_detector = false;
    double gftt_k = 0.64;

    // Actual size in meters
    float table_length = 1.15;
    float table_width = 0.70;
};

struct FrameCommands {
    bool retrack_table = false;
    bool regen_feature_detector = false;
    bool refen_gtff_detector = false;
    bool new_ref = false;
    bool new_mask = false;
};

#endif // FRAMESETTINGS_H
