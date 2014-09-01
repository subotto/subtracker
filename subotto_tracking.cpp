#include "subotto_tracking.hpp"
#include "subotto_metrics.hpp"
#include "utility.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/videostab/videostab.hpp>

#include <iostream>
#include <tuple>

#include "control.hpp"

using namespace std;
using namespace cv;
using namespace cv::videostab;

static Mat correctDistortion(Mat frameDistorted) {
//	Matx<float, 3, 3> cameraMatrix(160, 0, 160, 0, 160, 120, 0, 0, 1);
//	float k = -0.025;
//
//	Mat frame;
//	undistort(frameDistorted, frame, cameraMatrix, vector<float> {k, 0, 0, 0}, getDefaultNewCameraMatrix(cameraMatrix));

	return frameDistorted;
}

static Point_<float> applyTransform(Point_<float> p, Mat m) {
	Mat pm = (Mat_<float>(3, 1) << p.x, p.y, 1);
	Mat tpm = m * pm;
	float w = tpm.at<float>(2, 0);
	return Point_<float>(tpm.at<float>(0, 0) / w, tpm.at<float>(1, 0) / w);
}

static tuple< vector< KeyPoint >, Mat > get_features(Mat frame, Mat mask, int features_per_level, int features_levels) {

  Ptr< GoodFeaturesToTrackDetector > gftd(new GoodFeaturesToTrackDetector(features_per_level));
  PyramidAdaptedFeatureDetector fd(gftd, features_levels);
  vector< KeyPoint > features;
  fd.detect(frame, features, mask);

  Ptr< BriefDescriptorExtractor > bde(new BriefDescriptorExtractor(64));
	OpponentColorDescriptorExtractor de(bde);
  Mat features_descriptions;
  de.compute(frame, features, features_descriptions);

  return make_tuple(features, features_descriptions);

}

static Mat detect_table(Mat frame, table_detection_params_t& params, control_panel_t& panel, const SubottoReference& reference, const SubottoMetrics &metrics) {

	const Mat& reference_image = reference.image;
	const Mat& reference_mask = reference.mask;
	auto& reference_metrics = reference.metrics;

  vector< KeyPoint > frame_features, reference_features;
	Mat frame_features_descriptions, reference_features_descriptions;
	tie(frame_features, frame_features_descriptions) = get_features(frame, Mat(), params.frame_features_per_level, params.frame_features_levels);
  tie(reference_features, reference_features_descriptions) = get_features(reference_image, reference_mask, params.reference_features_per_level, params.reference_features_levels);

	vector<vector<DMatch>> matches_groups;

	BFMatcher dm;
	dm.knnMatch(reference_features_descriptions, frame_features_descriptions, matches_groups, params.features_knn, Mat());

	if(will_show(panel, "table detect", "matches")) {
		Mat matches;
		drawMatches(reference_image, reference_features, frame, frame_features, matches_groups, matches);
		show(panel, "table detect", "matches", matches);
	}

	vector<Point2f> coarse_from, coarse_to;

	for (auto matches : matches_groups) {
		for (DMatch match : matches) {
			auto f = reference_features[match.queryIdx].pt;
			auto t = frame_features[match.trainIdx].pt;

			coarse_from.push_back(f);
			coarse_to.push_back(t);
		}
	}

	logger(panel, "table detect", INFO) <<
			"reference features: " << reference_features.size() <<
			" frame features: " << frame_features.size() <<
			" matches: " << coarse_from.size() << endl;

	Mat coarse_transform;
	if(coarse_from.size() < 6) {
		coarse_transform = Mat::eye(3, 3, CV_32F);
		logger(panel, "table detect", WARNING) << "phase 1 motion estimation - not enough features!" << endl;
	} else {
		RansacParams ransac_params(6, params.coarse_ransac_threshold, params.coarse_ransac_outliers_ratio, 0.99f);
		float rmse;
		int ninliers;
		coarse_transform = estimateGlobalMotionRobust(coarse_from, coarse_to, LINEAR_SIMILARITY, ransac_params, &rmse, &ninliers);

		logger(panel, "table detect", INFO) <<
				"phase 1 motion estimation - rmse: " << rmse <<
				" inliers: " << ninliers << "/" << coarse_from.size() << endl;

	}

	Mat warped;
	warpPerspective(frame, warped, coarse_transform, reference_image.size(), WARP_INVERSE_MAP | INTER_LINEAR);

	show(panel, "table detect", "after feature matching", warped);
  show(panel, "table detect", "reference image", reference_image);

	vector<KeyPoint> optical_flow_features;

	PyramidAdaptedFeatureDetector optical_flow_fd(new GoodFeaturesToTrackDetector(params.optical_flow_features_per_level), params.optical_flow_features_levels);

	optical_flow_fd.detect(reference_image, optical_flow_features);

	SparsePyrLkOptFlowEstimator ofe;

	vector<Point2f> optical_flow_from, optical_flow_to;
	vector<uchar> status;

	for(KeyPoint kp : optical_flow_features) {
		optical_flow_from.push_back(kp.pt);
	}

	vector<Point2f> good_optical_flow_from, good_optical_flow_to;

	if (!optical_flow_features.empty()) {
		ofe.run(reference_image, warped, optical_flow_from, optical_flow_to, status, noArray());

		for (int i = 0; i < optical_flow_from.size(); i++) {
			if (!status[i]) {
				continue;
			}

			good_optical_flow_from.push_back(optical_flow_from[i]);
			good_optical_flow_to.push_back(optical_flow_to[i]);
		}

		logger(panel, "table detect", INFO) <<
				"detection optical flow features: " << good_optical_flow_from.size() << "/" << optical_flow_from.size() << endl;
	} else {
		logger(panel, "table detect", WARNING) << "detection optical flow - no features!" << endl;
	}

	Mat flow_correction;
	if (good_optical_flow_from.size() < 6) {
		flow_correction = Mat::eye(3, 3, CV_32F);
		logger(panel, "table detect", WARNING) << "detection optical flow - not enough features for flow correction!" << endl;
	} else {
		findHomography(good_optical_flow_from, good_optical_flow_to, RANSAC, params.optical_flow_ransac_threshold).convertTo(
				flow_correction, CV_32F);
	}

	Mat flow_transform = coarse_transform * flow_correction;

  logger(panel, "gio", VERBOSE) << endl << referenceToSize(reference_metrics, metrics) << endl;
	Mat transform = flow_transform * referenceToSize(reference_metrics, metrics);

	return transform;
}

static Mat follow_table(Mat frame, Mat previous_transform, table_following_params_t& params, table_tracking_status_t& status, control_panel_t& panel, const SubottoReference& reference, const SubottoMetrics &metrics) {
	Mat scaled_reference_image = status.scaled_reference;

	Size size = scaled_reference_image.size();

	auto& reference_metrics = reference.metrics;

	Mat warped;

	dump_time(panel, "cycle", "follow start");

	warpPerspective(frame, warped,
			previous_transform
					* sizeToReference(reference_metrics, metrics),
					size, WARP_INVERSE_MAP | INTER_LINEAR);

	dump_time(panel, "cycle", "follow warp");

	if(will_show(panel, "table detect", "follow table before"))
		show(panel, "table detect", "follow table before", warped);

	vector<KeyPoint> optical_flow_features = status.reference_features;

	SparsePyrLkOptFlowEstimator ofe;

	vector<Point2f> optical_flow_from, optical_flow_to;
	vector<uchar> optical_flow_status;

	for(KeyPoint kp : optical_flow_features) {
		optical_flow_from.push_back(kp.pt);
	}

	if(!optical_flow_from.empty()) {
		ofe.run(scaled_reference_image, warped, optical_flow_from, optical_flow_to, optical_flow_status, noArray());
	}

	dump_time(panel, "cycle", "follow optical flow");

	vector<Point2f> good_optical_flow_from, good_optical_flow_to;

	for (int i = 0; i < optical_flow_from.size(); i++) {
		if (!optical_flow_status[i]) {
			continue;
		}

		good_optical_flow_from.push_back(optical_flow_from[i]);
		good_optical_flow_to.push_back(optical_flow_to[i]);
	}

	Mat correction;
	if (good_optical_flow_from.size() < 6) {
		correction = Mat::eye(3, 3, CV_32F);
	} else {
		int ninliers;
		float rmse;
		correction = estimateGlobalMotionRobust(optical_flow_from, optical_flow_to,
				LINEAR_SIMILARITY,
				RansacParams(6, params.optical_flow_ransac_threshold, 0.6f, 0.99f), &rmse,
				&ninliers);
	}

	dump_time(panel, "cycle", "follow motion estimation");

	return previous_transform * sizeToReference(reference_metrics, metrics) * correction * referenceToSize(reference_metrics, metrics);
}

void init_table_tracking_panel(table_tracking_params_t& params, control_panel_t& panel) {
	trackbar(panel, "table detect", "detect every", params.detect_every_frames, {0, 1000, 1});

	trackbar(panel, "table detect", "coarse reference features per level", params.detection.reference_features_per_level, {0, 1000, 1});
	trackbar(panel, "table detect", "coarse reference features levels", params.detection.reference_features_levels, {0, 10, 1});
	trackbar(panel, "table detect", "coarse frame features per level", params.detection.frame_features_per_level, {0, 1000, 1});
	trackbar(panel, "table detect", "coarse frame features levels", params.detection.frame_features_levels, {0, 10, 1});
	trackbar(panel, "table detect", "coarse features knn", params.detection.features_knn, {0, 10, 1});
	trackbar(panel, "table detect", "coarse ransac threshold", params.detection.coarse_ransac_threshold, {0.0f, 100.f, 0.1f});
	trackbar(panel, "table detect", "coarse ransac outliers ratio", params.detection.coarse_ransac_outliers_ratio, {0.0f, 1.f, 0.01f});
	trackbar(panel, "table detect", "optical flow features", params.detection.optical_flow_features_per_level, {0, 1000, 1});
	trackbar(panel, "table detect", "optical flow features levels", params.detection.optical_flow_features_levels, {0, 10, 1});
	trackbar(panel, "table detect", "optical flow ransac threshold", params.detection.optical_flow_ransac_threshold, {0.0f, 10.f, 0.1f});

	trackbar(panel, "table detect", "follow optical flow features", params.following_params.optical_flow_features, {0, 1000, 1});
	trackbar(panel, "table detect", "follow optical flow ransac threshold", params.following_params.optical_flow_ransac_threshold, {0.0f, 100.f, 0.1f});
}

Mat track_table(Mat frame, table_tracking_status_t& status, table_tracking_params_t& params, control_panel_t& panel, const SubottoReference& reference, const SubottoMetrics &metrics) {
	Mat undistorted = correctDistortion(frame);

	Mat transform;

	if (status.near_transform.empty() || status.frames_to_next_detection <= 0) {
		transform = detect_table(undistorted, params.detection, panel, reference, metrics);
		status.frames_to_next_detection = params.detect_every_frames;
		status.near_transform = transform;
	} else {
		transform = follow_table(undistorted, status.near_transform, params.following_params, status, panel, reference, metrics);
		status.frames_to_next_detection--;
		// smooth the previous transform
		accumulateWeighted(transform, status.near_transform, params.near_transform_alpha);
	}

	return transform;
}

void drawSubottoBorders(Mat& outImage, const Mat& transform, Scalar color) {
	vector<Point_<float>> subottoCorners = { Point_<float>(-1, -0.6), Point_<
			float>(-1, +0.6), Point_<float>(+1, +0.6), Point_<float>(+1, -0.6) };

	for (int corner = 0; corner < subottoCorners.size(); corner++) {
		subottoCorners[corner] = applyTransform(subottoCorners[corner],
				transform);
	}

	for (int corner = 0; corner < subottoCorners.size(); corner++) {
		line(outImage, subottoCorners[corner],
				subottoCorners[(corner + 1) % subottoCorners.size()], color, 1, 16);
	}
}
