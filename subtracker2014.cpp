#include <iostream>
#include <iterator>
#include <string>
#include <deque>

#include <cmath>
#include <chrono>

#include "utility.hpp"
#include "blobs_tracker.hpp"
#include "subotto_metrics.hpp"
#include "subotto_tracking.hpp"
#include "control.hpp"
#include "staging.hpp"
#include "analysis.hpp"

using namespace cv;
using namespace std;
using namespace chrono;

SubottoReference reference;
SubottoMetrics metrics;

FoosmenMetrics foosmenMetrics;

control_panel_t panel;

BlobsTracker blobs_tracker(panel);

void getTableFrame(Mat frame, Mat& tableFrame, Size size, Mat transform) {
	Mat frame32f;
	frame.convertTo(frame32f, CV_32F, 1 / 255.f);

	Mat warpTransform = transform * sizeToUnits(metrics, size);
	warpPerspective(frame32f, tableFrame, warpTransform, size, CV_WARP_INVERSE_MAP | CV_INTER_LINEAR);
}

vector<pair<Point2f, float>> findLocalMaxima(Mat density, int radiusX, int radiusY, int limit) {
	typedef pair<Point, float> pi; // point, integer
	typedef pair<Point2f, float> pf; // point, floating point

	Mat dilatedDensity;
	dilate(density, dilatedDensity, Mat::ones(2 * radiusY + 1, 2 * radiusX + 1, CV_8U));

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

void doIt(FrameReader& frameReader) {
	Mat trajReprAvg;

	bool play = true;

	int timeline_span = 120;
	int processed_frames = 60;	// number of frames to be processed for each call to ProcessFrame

	int current_time = 0;
	int initial_time = 0;

	deque<Mat> frames;	// used for display only

	Size tableFrameSize(128, 64);

	TableDescription table(tableFrameSize);
	BallDescription ball;

	TableAnalysis tableAnalysis;
	BallAnalysis ballAnalysis;

	FoosmenBarMetrics barsMetrics[BARS][2];
	FoosmenBarAnalysis barsAnalysis[BARS][2];

	Mat density;

	deque< vector<float> > foosmenValues;	// Values to be printed for each frame
	deque<double> timestamps;				// Timestamp of each frame

  table_tracking_params_t table_tracking_params;
	init_table_tracking_panel(table_tracking_params, panel);
	table_tracking_status_t table_tracking_status(table_tracking_params, reference);

	foosmen_params_t foosmen_params;

	int local_maxima_limit = 5;
	float local_maxima_min_distance = 0.10f;

	trackbar(panel, "foosmen tracking", "rot factor", foosmen_params.nll_threshold, {0.f, 10.f, 0.1});

	trackbar(panel, "foosmen tracking", "convolution width", foosmen_params.convolution_width, {0, 0.1, 0.001});
	trackbar(panel, "foosmen tracking", "convolution length", foosmen_params.convolution_length, {0, 0.2, 0.001});

	trackbar(panel, "foosmen tracking", "window length", foosmen_params.window_length, {0, 0.2, 0.001});
	trackbar(panel, "foosmen tracking", "goalkeeper window length", foosmen_params.goalkeeper_window_length, {0, 0.2, 0.001});

	trackbar(panel, "foosmen tracking", "rot factor", foosmen_params.rot_factor, {0, 100, 0.1});

	color_picker(panel, "foosmen colors", "red color mean", foosmen_params.mean_color[0]);
	color_picker(panel, "foosmen colors", "blue color mean", foosmen_params.mean_color[1]);

	color_qf_picker(panel, "foosmen colors", "red color precision", foosmen_params.color_precision[0]);
	color_qf_picker(panel, "foosmen colors", "blue color precision", foosmen_params.color_precision[1]);

	trackbar(panel, "reference metrics", "width", reference.metrics.frameSize.width, {-5.f, 5.f, 0.001f});
	trackbar(panel, "reference metrics", "height", reference.metrics.frameSize.height, {-5.f, 5.f, 0.001f});

	trackbar(panel, "reference metrics", "x", reference.metrics.offset.x, {-0.1f, +0.1f, 0.001f});
	trackbar(panel, "reference metrics", "y", reference.metrics.offset.y, {-0.1f, +0.1f, 0.001f});

	trackbar(panel, "ball tracking", "local_maxima limit", local_maxima_limit, {0, 1000, 1});
	trackbar(panel, "ball tracking", "local maxima min distance", local_maxima_min_distance, {0, 200, 0.1f});

	trackbar(panel, "foosmen metrics", "goalkeeper x", foosmenMetrics.barx[GOALKEEPER], {-2.f, 2.f, 0.001f});
	trackbar(panel, "foosmen metrics", "rod 2 x", foosmenMetrics.barx[BAR2], {-2.f, 2.f, 0.001f});
	trackbar(panel, "foosmen metrics", "rod 5 x", foosmenMetrics.barx[BAR5], {-2.f, 2.f, 0.001f});
	trackbar(panel, "foosmen metrics", "rod 3 x", foosmenMetrics.barx[BAR3], {-2.f, 2.f, 0.001f});

	trackbar(panel, "foosmen metrics", "goalkeeper gap", foosmenMetrics.distance[GOALKEEPER], {0.f, 2.f, 0.001f});
	trackbar(panel, "foosmen metrics", "rod 2 gap", foosmenMetrics.distance[BAR2], {0.f, 2.f, 0.001f});
	trackbar(panel, "foosmen metrics", "rod 5 gap", foosmenMetrics.distance[BAR5], {0.f, 2.f, 0.001f});
	trackbar(panel, "foosmen metrics", "rod 3 gap", foosmenMetrics.distance[BAR3], {0.f, 2.f, 0.001f});

	vector<Point2f> previous_positions;
	int previous_positions_start_time;
	vector<Mat> previous_frames;

	int wait_key_skip = 5;

	trackbar(panel, "control panel", "update display skip frames", wait_key_skip, {0, 20, 1});
	for (int i = 0; ; i++) {
		panel.update_display = (i % (wait_key_skip + 1) == 0);

		if (panel.update_display) {
			namedWindow("control panel", WINDOW_NORMAL);

			int c = waitKey(1);
      if (c > 0) c &= 0xff;
      if (c != -1) {
        logger(panel, "gio", DEBUG) << "key: " << c << endl;
      }

			switch(c) {
			case 'r':
				set_log_level(panel, "capture", VERBOSE);
				break;
			case 'p':
				toggle(panel, "control panel", TRACKBAR);
				break;
			case 'f':
				toggle(panel, "frame", SHOW);
				break;
			case 'c':
				toggle(panel, "cycle", TIME);
				break;
			case 's':
				set_log_level(panel, "table detect", VERBOSE);
				toggle(panel, "table detect", TRACKBAR, true);
        toggle(panel, "table detect", SHOW, true);
				break;
			case 'b':
				set_log_level(panel, "ball tracking", VERBOSE);
				toggle(panel, "ball tracking", TRACKBAR, true);
				toggle(panel, "ball tracking", SHOW, true);
				break;
			case 'm':
				toggle(panel, "foosmen tracking", TRACKBAR);
				toggle(panel, "foosmen tracking", SHOW);
				break;
			case 'l':
				toggle(panel, "foosmen metrics", TRACKBAR);
				toggle(panel, "reference metrics", TRACKBAR);
				break;
			case 'n':
				toggle(panel, "foosmen colors", TRACKBAR);
				break;
			case ' ':
				set_log_level(panel, "table detect", WARNING);
				set_log_level(panel, "ball tracking", WARNING);
        set_log_level(panel, "capture", WARNING);

				toggle(panel, "table detect", TRACKBAR, false);
				toggle(panel, "ball tracking", TRACKBAR, false);
				toggle(panel, "ball tracking", SHOW, false);

				toggle(panel, "foosmen tracking", TRACKBAR, false);
				toggle(panel, "foosmen tracking", SHOW, false);

				toggle(panel, "foosmen metrics", TRACKBAR, false);
				toggle(panel, "reference metrics", TRACKBAR, false);

				toggle(panel, "foosmen colors", TRACKBAR, false);

				toggle(panel, "frame", SHOW, false);
				toggle(panel, "cycle", TIME, false);
				toggle(panel, "control panel", TRACKBAR, false);
				break;
			}

			dump_time(panel, "cycle", "wait key");
		}

		dump_time(panel, "cycle", "start cycle");

		auto frame_info = frameReader.get();
    if (!frame_info.valid) break;
		Mat frame = frame_info.data;

    // Match the position of the table against the stored reference
		Mat table_transform = track_table(frame_info.data, table_tracking_status, table_tracking_params, panel, reference);

		dump_time(panel, "cycle", "detect subotto");

		show(panel, "frame", "frame", frame);

		if ( current_time >= timeline_span ) {
			timestamps.push_back(duration_cast<duration<double>>(frame_info.playback_time.time_since_epoch()).count());
		}

		Mat tableFrame;
		getTableFrame(frame, tableFrame, tableFrameSize, table_transform);

    show(panel, "frame", "tableFrame", tableFrame);

		dump_time(panel, "cycle", "warp table frame");

    do_table_analysis(panel,
                      tableFrame,
                      table,
                      tableAnalysis);

    do_ball_analysis(panel,
                     tableFrame,
                     ball,
                     tableAnalysis,
                     ballAnalysis,
                     density);

    do_update_table_description(panel,
                                tableFrame,
                                tableAnalysis,
                                table);

    if (i % 5 == 0) {
      do_update_corrected_variance(panel,
                                   table);
    }

    do_foosmen_analysis(panel,
                        barsMetrics,
                        barsAnalysis,
                        metrics,
                        foosmenMetrics,
                        tableFrameSize,
                        foosmen_params,
                        tableFrame,
                        tableAnalysis,
                        current_time,
                        timeline_span,
                        foosmenValues);

		int radiusX = local_maxima_min_distance / metrics.length * tableFrameSize.width;
		int radiusY = local_maxima_min_distance / metrics.width * tableFrameSize.height;

		logger(panel, "ball tracking", DEBUG) << "LM radius x: " << radiusX << "LM radius y: " << radiusY << endl;

		auto localMaxima = findLocalMaxima(density, radiusX, radiusY, local_maxima_limit);

		dump_time(panel, "cycle", "find local maxima");

		vector<Blob> blobs;

		// Cambio le unità di misura secondo le costanti in SubottoMetrics
		for (int i=0; i<localMaxima.size(); i++) {
			Point2f &p = localMaxima[i].first;

			p.x =  (p.x / density.cols - 0.5f) * metrics.length;
			p.y = -(p.y / density.rows - 0.5f) * metrics.width;
		}

		// Inserisco i punti migliori come nuovo frame nella timeline
		for (int i=0; i<localMaxima.size(); i++) {
			blobs.push_back( Blob(localMaxima[i].first, 0.0, 0.0, localMaxima[i].second) );
		}

		if(is_loggable(panel, "ball tracking", DEBUG))
			logger(panel, "ball tracking", DEBUG) << "Inserting frame " << current_time << " in timeline" << endl;

    blobs_tracking(panel,
                   current_time,
                   initial_time,
                   timeline_span,
                   processed_frames,
                   blobs_tracker,
                   timestamps,
                   foosmenValues,
                   frames,
                   previous_positions,
                   previous_positions_start_time,
                   previous_frames,
                   blobs,
                   tableFrame);

    display_ball(panel,
                 current_time,
                 previous_positions,
                 previous_positions_start_time,
                 previous_frames,
                 metrics,
                 density);

	}
}

int main(int argc, char* argv[]) {
	string videoName, referenceImageName, referenceImageMaskName;
	if (argc == 3 || argc == 4) {
		videoName = argv[1];
		referenceImageName = argv[2];

		if(argc == 4) {
			referenceImageMaskName = argv[3];
		}
	} else {
		cerr << "Usage: " << argv[0] << " <video> <reference subotto> [<reference subotto mask>]" << endl;
		cerr << "<video> can be: " << endl;
		cerr << "\tn (a single digit number) - live capture from video device n" << endl;
		cerr << "\tfilename+ (with a trailing plus) - simulate live capture from video file" << endl;
		cerr << "\tfilename (without a trailing plus) - batch analysis of video file" << endl;
		cerr << "<reference subotto> is the reference image used to look for the table" << endl;
		cerr << "<reference sumotto mask> is an optional B/W image, of the same size," << endl;
		cerr << "\twhere black spots indicate areas of the reference image to hide" << endl;
		cerr << "\twhen looking for the table (such as moving parts and spurious features)" << endl;
		return 1;
	}

	reference.image = imread(referenceImageName);

	if(!referenceImageMaskName.empty()) {
		reference.mask = imread(referenceImageMaskName, CV_LOAD_IMAGE_GRAYSCALE);
	}

	init_control_panel(panel);
  set_log_level(panel, "gio", VERBOSE);

	if(videoName.size() == 1) {
		FrameReader f(videoName[0] - '0', panel);
		doIt(f);
	} else {
		bool simulate_live = false;

		if(videoName.back() == '+') {
			videoName = videoName.substr(0, videoName.size()-1);
			simulate_live = true;
		}

		FrameReader f(videoName.c_str(), panel, simulate_live);
		doIt(f);
	}

	return 0;
}
