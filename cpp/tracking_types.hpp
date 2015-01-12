
#ifndef TRACKING_TYPES_H_
#define TRACKING_TYPES_H_

#include "subotto_metrics.hpp"
#include "framereader.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/videostab/videostab.hpp>

struct SubottoReference {
	cv::Mat image;
	cv::Mat mask;
	SubottoReferenceMetrics metrics;
};

struct table_detection_params_t {
	int reference_features_per_level;
	int reference_features_levels;

	int frame_features_per_level;
	int frame_features_levels;

	int features_knn;

	float coarse_ransac_threshold;
	float coarse_ransac_outliers_ratio;

	int optical_flow_features_per_level;
	int optical_flow_features_levels;

	float optical_flow_ransac_threshold;

  table_detection_params_t();
};

struct table_following_params_t {
	int optical_flow_features;
	float optical_flow_ransac_threshold;

  table_following_params_t();
};

struct table_tracking_params_t {
	table_detection_params_t detection;
	table_following_params_t following_params;

	int detect_every_frames;
	float near_transform_alpha;

  table_tracking_params_t();
};

struct table_tracking_status_t {
	cv::Mat near_transform;
	int frames_to_next_detection;

	cv::Mat scaled_reference;
	std::vector<cv::KeyPoint> reference_features;

  table_tracking_status_t(const table_tracking_params_t& params, const SubottoReference& reference, Size table_frame_size);
};

struct table_tracking_t {
	FrameInfo frameInfo;
	cv::Mat frame;
	cv::Mat transform;
};

#endif
