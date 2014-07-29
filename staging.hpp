#ifndef STAGING_HPP_
#define STAGING_HPP_

#include <deque>

using namespace std;
using namespace cv;

void blobs_tracking(control_panel_t &panel,
                    int &current_time,
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
                  int &current_time,
                  vector<Point2f> &previous_positions,
                  int &previous_positions_start_time,
                  vector<Mat> &previous_frames,
                  SubottoMetrics &metrics,
                  Mat &density);

vector<pair<Point2f, float>> findLocalMaxima(control_panel_t &panel, Mat density, int radiusX, int radiusY, int limit);

#endif /* STAGING_HPP_ */
