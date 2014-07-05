//#include <iostream>
//#include <iterator>
//#include <string>
#include <deque>

//#include <cmath>
//#include <chrono>

//#include "utility.hpp"
//#include "blobs_finder.hpp"
#include "blobs_tracker.hpp"
//#include "subotto_metrics.hpp"
//#include "subotto_tracking.hpp"
//#include "control.hpp"
#include "staging.hpp"

using namespace cv;
using namespace std;
//using namespace chrono;


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
