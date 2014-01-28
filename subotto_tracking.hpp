#ifndef SUBOTTODETECTOR_H_
#define SUBOTTODETECTOR_H_

#include "subotto_metrics.hpp"
#include "framereader.hpp"
#include "control.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/videostab/videostab.hpp>

#include <memory>

struct SubottoReference {
	cv::Mat image;
	cv::Mat mask;
	SubottoReferenceMetrics metrics;
};

struct table_detection_params_t {
	int reference_features_per_level = 300;
	int reference_features_levels = 3;

	int frame_features_per_level = 500;
	int frame_features_levels = 3;

	int features_knn = 2;

	float coarse_ransac_threshold = 20.f;
	float coarse_ransac_outliers_ratio = 0.75f;

	int optical_flow_features_per_level = 200;
	int optical_flow_features_levels = 3;

	float optical_flow_ransac_threshold = 1.0f;

	const SubottoReference* reference;
};

struct table_following_params_t {
	int optical_flow_features = 100;
	float optical_flow_ransac_threshold = 1.0f;

	cv::Size optical_flow_size {128, 64};

	const SubottoReference* reference;
};

struct table_tracking_params_t {
	table_detection_params_t detection;
	table_following_params_t following_params;

	int detect_every_frames = 120;
	float near_transform_alpha = 0.25f;
};

struct table_tracking_status_t {
	cv::Mat near_transform;
	int frames_to_next_detection;

	cv::Mat scaled_reference;
	std::vector<cv::KeyPoint> reference_features;
};

struct table_tracking_t {
	frame_info frameInfo;
	cv::Mat frame;
	cv::Mat transform;
};

Mat detect_table(Mat frame, table_detection_params_t& params, control_panel_t& panel);
Mat follow_table(Mat frame, Mat previous_transform, table_following_params_t& params, table_tracking_status_t& status, control_panel_t& panel);

void init_table_tracking(table_tracking_status_t& status, table_tracking_params_t& params, control_panel_t& panel);
Mat track_table(Mat frame, table_tracking_status_t& status, table_tracking_params_t& params, control_panel_t& panel);

void drawSubottoBorders(cv::Mat& outImage, const cv::Mat& transform, cv::Scalar color);

#endif /* SUBOTTODETECTOR_H_ */
