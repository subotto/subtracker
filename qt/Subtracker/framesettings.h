#ifndef FRAMESETTINGS_H
#define FRAMESETTINGS_H

#include <opencv2/core/core.hpp>

#include <chrono>

#include "framereader.h"

struct FrameSettings {
    // The four corners of the table, in this order: red defence, red attack, blue defence, blue attack
    //cv::Point2f table_corners[4] = { { 100.0, 400.0 }, { 600.0, 400.0 }, { 600.0, 100.0 }, { 100.0, 100.0 } };
    //std::vector< cv::Point2f > ref_corners = {{ 119.0, 382.0 }, { 547.0, 379.0 }, { 550.0, 122.0 }, { 116.0, 124.0 }};
    std::vector< cv::Point2f > ref_corners = {{ 131.0, 361.0 }, { 498.0, 338.0 }, { 482.0, 115.0 }, { 117.0, 141.0 }};
    float intermediate_alpha = 1.0;
    //cv::Size intermediate_size = { 400, 300 };
    // OpenCV uses BGR colors; indexes are: 0 -> red foosmen, 1 -> blue foosmen, 2 -> ball
    cv::Scalar objects_colors[3] = { { 0.0f, 0.0f, 1.0f}, { 1.0f, 0.0f, 0.0f }, { 0.85f, 0.85f, 0.85f } };
    float objects_color_stddev[3] = { 0.15f, 0.15f, 0.075f };
    cv::Scalar initial_mean = { 135.0/255.0, 150.0/255.0, 120.0/255.0 };
    cv::Scalar initial_var = { (20.0/255.0)*(20.0/255.0), (20.0/255.0)*(20.0/255.0), (20.0/255.0)*(20.0/255.0) };

    cv::Mat ref_image;
    std::string ref_image_filename;
    cv::Mat ref_mask;
    std::string ref_mask_filename;
    cv::Mat camera_matrix, distortion_coefficients, calibration_map1, calibration_map2;
    std::string camera_parameters_filename;

    // Retracking times
    FrameClock::duration surf_interval = std::chrono::milliseconds(5000);
    FrameClock::duration of_interval = std::chrono::milliseconds(1000);

    // SURF
    int feats_hessian_threshold = 600;
    int feats_n_octaves = 10;
    int feats_ransac_threshold = 20;
    float det_threshold = 0.1;

    // Optical flow features
    int gftt_max_corners = 100;
    double gftt_quality_level = 0.01;
    double gftt_min_distance = 3.0;
    int gftt_blockSize = 3;
    bool gftt_use_harris_detector = false;
    double gftt_k = 0.64;

    // Optical flow
    double of_win_side = 21;
    int of_max_level = 3;
    int of_term_count = 30;
    int of_term_eps = 0.01;
    int of_ransac_threshold = 3;

    // Foosmen detection
    double foosmen_strip_width = 0.1;
    int foosmen_blur_size = 10;

    // Running average
    float accumul_coeff = 0.002f;

    // Ball detection
    float table_nll_threshold = 20.0;
    uint8_t ball_blur_size = 6;
    uint8_t maxima_count = 5;
    uint8_t maxima_radius = 15;

    // Spots tracking
    uint32_t spots_tracking_len = 200;

    // Actual size in meters (not all of them are actually used in computation)
    float table_length = 1.14f;
    float table_width = 0.71f;
    float camera_heigth = 1.93f;
    float rod_height = 0.085f;
    float field_rods_dist_coeff = camera_heigth / (camera_heigth - rod_height);
    float ball_diameter = 0.035f;
    float rod_diameter = field_rods_dist_coeff * 0.017f;
    float rod_distance = field_rods_dist_coeff * 0.151f;
    float foosman_width = field_rods_dist_coeff * 0.034f;
    float foosman_head = 0.040f;
    float foosman_foot = 0.073f;
    struct RodParams {
        uint8_t num;
        bool gk;
        float dist;
        uint8_t side;
    };
    RodParams rod_configuration[8] = {
        { 3, true, field_rods_dist_coeff * 0.415f / 2.0f, 0 },
        { 2, false, field_rods_dist_coeff * 0.238f, 0 },
        { 3, false, field_rods_dist_coeff * 0.416f / 2.0f, 1 },
        { 5, false, field_rods_dist_coeff * 0.476f / 4.0f, 0 },
        { 5, false, field_rods_dist_coeff * 0.476f / 4.0f, 1 },
        { 3, false, field_rods_dist_coeff * 0.416f / 2.0f, 0 },
        { 2, false, field_rods_dist_coeff * 0.238f, 1 },
        { 3, true, field_rods_dist_coeff * 0.415f / 2.0f, 1 }
    };
    static constexpr size_t rod_num = sizeof(rod_configuration) / sizeof(rod_configuration[0]);
    static constexpr uint8_t team_num = 2;
    static constexpr uint8_t rod_per_team = 4;
    static_assert(rod_num == team_num * rod_per_team);
    inline uint8_t resolve_rod(uint8_t team, uint8_t num) const {
        uint8_t res;
        switch (num) {
        case 0: res = 0; break;
        case 1: res = 1; break;
        case 2: res = 3; break;
        case 3: res = 5; break;
        default: throw "Wrong rod num";
        }
        if (team != 0) {
            res = 7 - res;
        }
        return res;
    }
    inline std::pair< uint8_t, uint8_t > rresolve_rod(uint8_t num) const {
        switch (num) {
        case 0: return std::make_pair(0, 0);
        case 1: return std::make_pair(0, 1);
        case 2: return std::make_pair(1, 3);
        case 3: return std::make_pair(0, 2);
        case 4: return std::make_pair(1, 2);
        case 5: return std::make_pair(0, 4);
        case 6: return std::make_pair(1, 1);
        case 7: return std::make_pair(1, 0);
        default: throw "Wrong team rod num";
        }
    }
};

struct FrameCommands {
    bool new_ref = false;
    bool new_mask = false;
    bool regen_feature_detector = false;
    bool refen_gtff_detector = false;
    bool redetect_features = false;
    bool retrack_table = false;
};

#endif // FRAMESETTINGS_H
