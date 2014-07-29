#include <deque>

#include "opencv2/imgproc/imgproc.hpp"

#include "blobs_tracker.hpp"
#include "subotto_metrics.hpp"
#include "staging.hpp"
#include "analysis.hpp"

using namespace cv;
using namespace std;


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
                    const Mat &tableFrame) {

		blobs_tracker.InsertFrameInTimeline(blobs, current_time);
		
		// Metto da parte il frame per l'eventuale visualizzazione
		if ( current_time >= timeline_span ) {
			frames.push_back(tableFrame);
		}

		if ( current_time >= 2*timeline_span ) {
			blobs_tracker.PopFrameFromTimeline();
			int processed_time = initial_time + timeline_span;

			// Il processing vero e proprio avviene solo ogni k fotogrammi (k=processed_frames)
			if ( initial_time % processed_frames == 0 ) {
				vector<Point2f> positions = blobs_tracker.ProcessFrames( initial_time, processed_time, processed_time + processed_frames );

				for (int i=0; i<positions.size(); i++) {
					int frame_id = processed_time + i;
					
					assert(!timestamps.empty());
					double timestamp = timestamps.front();
					timestamps.pop_front();
					
					assert(!foosmenValues.empty());
					vector<float> foosmenValuesFrame = foosmenValues.front();
					foosmenValues.pop_front();

					printf("%lf", timestamp);
					
					// Controllo che la pallina sia stata effettivamente trovata (la posizione non deve essere NISBA) e la stampo
					if ( positions[i].x < 900.0 && positions[i].y < 900.0 ) printf(",%lf,%lf", positions[i].x, positions[i].y);
					else printf(",,");
					
					for (int j=0; j<foosmenValuesFrame.size(); j++) {
						printf(",%f", foosmenValuesFrame[j]);
					}
					printf("\n");
					fflush(stdout);
				}

				previous_positions = positions;
				previous_positions_start_time = current_time;

				previous_frames.clear();
				previous_frames.resize(frames.size());
				copy(frames.begin(), frames.end(), previous_frames.begin());

				for (int i=0; i<processed_frames; i++) {
					frames.pop_front();
				}
			}

			initial_time++;
		}

		current_time++;

		dump_time(panel, "cycle", "blobs tracking");

}

void display_ball(control_panel_t &panel,
                  int &current_time,
                  vector<Point2f> &previous_positions,
                  int &previous_positions_start_time,
                  vector<Mat> &previous_frames,
                  SubottoMetrics &metrics,
                  Mat &density) {

		int relative_time = current_time - previous_positions_start_time;

		if (will_show(panel, "ball tracking", "ball") &&
			!previous_positions.empty() &&
			relative_time < previous_positions.size() &&
			relative_time >= 0) {
			Mat display;

			previous_frames[relative_time].copyTo(display);

			Point2f ball = previous_positions[relative_time];

			ball.x = (ball.x / metrics.length + 0.5f) * density.cols;
			ball.y = (ball.y / metrics.width + 0.5f) * density.rows;

			circle( display, ball, 8, Scalar(0,255,0), 2 );

			show(panel, "ball tracking", "ball", display);

			dump_time(panel, "cycle", "display ball");
		}

}

vector<pair<Point2f, float>> findLocalMaxima(control_panel_t &panel, Mat density, int radiusX, int radiusY, int limit) {
	typedef pair<Point, float> pi; // point, integer
	typedef pair<Point2f, float> pf; // point, floating point

	Mat dilatedDensity;
	dilate(density, dilatedDensity, Mat::ones(2 * radiusY + 1, 2 * radiusX + 1, CV_8U));

  show(panel, "frame", "density", density);
  show(panel, "frame", "dilatedDensiy", dilatedDensity);

	Mat localMaxMask = (density >= dilatedDensity);

	Mat_<Point> nonZero;
	findNonZero(localMaxMask, nonZero);

	vector<pi> localMaxima;
	for(int i = 0; i < nonZero.rows; i++) {
		Point p = *nonZero[i];
		float w = density.at<float>(p);

		localMaxima.push_back(make_pair(p, w));
	}

	int count = min(localMaxima.size(), size_t(limit));
	nth_element(localMaxima.begin(), localMaxima.begin() + count, localMaxima.end(), [](pi a, pi b) {
		return a.second > b.second;
	});
	localMaxima.resize(count);

	vector<pf> results;
	results.reserve(count);
	for(pi lm : localMaxima) {
		Point p = lm.first;

		// trova la posizione in modo più preciso
		Point2f correction = subpixelMinimum(panel, -density(Range(p.y, p.y+1), Range(p.x, p.x+1)));

		results.push_back(make_pair(Point2f(p.x, p.y) + correction, lm.second));
	}

	return results;
}
