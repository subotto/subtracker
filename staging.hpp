#ifndef STAGING_HPP_
#define STAGING_HPP_

#include <deque>

using namespace std;
using namespace cv;

void search_spots(control_panel_t &panel,
                  const Mat &density,
                  int local_maxima_limit,
                  float local_maxima_min_distance,
                  vector< Spot > &spots,
                  const SubottoMetrics &metrics,
                  const Size &tableFrameSize,
                  int current_time);

#endif /* STAGING_HPP_ */
