#ifndef STAGING_HPP_
#define STAGING_HPP_

#include <deque>

using namespace std;
using namespace cv;

void blobs_tracking(control_panel_t &panel,
                    const int current_time,
                    int &initial_time,
                    const int &timeline_span,
                    const int &processed_frames,
                    BlobsTracker &blobs_tracker,
                    deque<double> &timestamps,
                    deque< vector<float> > &foosmenValues,
                    deque<Mat> &frames,
                    vector<Point2f> &previous_positions,
                    int &previous_positions_start_time,
                    vector<Mat> &previous_frames,
                    const vector<Blob> &blobs,
                    const Mat &tableFrame);

void display_ball(control_panel_t &panel,
                  int current_time,
                  const vector<Point2f> &previous_positions,
                  const int &previous_positions_start_time,
                  const vector<Mat> &previous_frames,
                  const SubottoMetrics &metrics,
                  const Mat &density);

vector<pair<Point2f, float>> findLocalMaxima(control_panel_t &panel,
                                             Mat density,
                                             int radiusX,
                                             int radiusY,
                                             int limit);

void search_blobs(control_panel_t &panel,
                  const Mat &density,
                  int local_maxima_limit,
                  float local_maxima_min_distance,
                  vector< Blob > &blobs,
                  const SubottoMetrics &metrics,
                  const Size &tableFrameSize,
                  int current_time);

#endif /* STAGING_HPP_ */
