
#include "tracking_types.hpp"

table_detection_params_t::table_detection_params_t()
  : reference_features_per_level(100),
    reference_features_levels(3),

    frame_features_per_level(100),
    frame_features_levels(3),

    features_knn(2),

    coarse_ransac_threshold(20.f),
    coarse_ransac_outliers_ratio(0.75f),

    optical_flow_features_per_level(50),
    optical_flow_features_levels(3),

    optical_flow_ransac_threshold(1.0f)

{}

table_following_params_t::table_following_params_t()
  : optical_flow_features(50),
    optical_flow_ransac_threshold(1.0f)

{}

table_tracking_params_t::table_tracking_params_t()
  : detect_every_frames(120),
    near_transform_alpha(0.25f)

{}

table_tracking_status_t::table_tracking_status_t(const table_tracking_params_t& params, const SubottoReference& reference, const Size &table_frame_size)
  : frames_to_next_detection(0), params(params), reference(reference), table_frame_size(table_frame_size) {
}

void table_tracking_status_t::detect_features() {

  resize(this->reference.image, this->scaled_reference, this->table_frame_size);
  GFTTDetector::create(this->params.following_params.optical_flow_features)->detect(this->scaled_reference, this->reference_features);

}
