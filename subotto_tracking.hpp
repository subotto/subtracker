#ifndef SUBOTTOTRACKING_H_
#define SUBOTTOTRACKING_H_

#include "subotto_metrics.hpp"
#include "tracking_types.hpp"
#include "control.hpp"
#include "context.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/videostab/videostab.hpp>

#include <memory>

void init_table_tracking_panel(table_tracking_params_t& params, control_panel_t& panel);
Mat track_table(Mat frame, table_tracking_status_t& status, table_tracking_params_t& params, control_panel_t& panel, const SubottoReference& reference, const SubottoMetrics &metrics, FrameAnalysis &frame_analysis);

#endif /* SUBOTTOTRACKING_H_ */
