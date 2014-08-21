#include <deque>

#include "opencv2/imgproc/imgproc.hpp"

#include "blobs_tracker.hpp"
#include "subotto_metrics.hpp"
#include "staging.hpp"
#include "analysis.hpp"

using namespace cv;
using namespace std;


static vector<pair<Point2f, float>> findLocalMaxima(control_panel_t &panel, Mat density, int radiusX, int radiusY, int limit) {
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

void search_blobs(control_panel_t &panel,
                  const Mat &density,
                  int local_maxima_limit,
                  float local_maxima_min_distance,
                  vector< Blob > &blobs,
                  const SubottoMetrics &metrics,
                  const Size &tableFrameSize,
                  int current_time) {

		int radiusX = local_maxima_min_distance / metrics.length * tableFrameSize.width;
		int radiusY = local_maxima_min_distance / metrics.width * tableFrameSize.height;

		logger(panel, "ball tracking", DEBUG) << "LM radius x: " << radiusX << "LM radius y: " << radiusY << endl;

		auto localMaxima = findLocalMaxima(panel, density, radiusX, radiusY, local_maxima_limit);

		dump_time(panel, "cycle", "find local maxima");

		// Cambio le unità di misura secondo le costanti in SubottoMetrics
		for (int i=0; i<localMaxima.size(); i++) {
			Point2f &p = localMaxima[i].first;

			p.x =  (p.x / density.cols - 0.5f) * metrics.length;
			p.y = -(p.y / density.rows - 0.5f) * metrics.width;
		}

		// Inserisco i punti migliori come nuovo frame nella timeline
		for (int i=0; i<localMaxima.size(); i++) {
			blobs.emplace_back(localMaxima[i].first, 0.0, 0.0, localMaxima[i].second);
		}

		if(is_loggable(panel, "ball tracking", DEBUG))
			logger(panel, "ball tracking", DEBUG) << "Inserting frame " << current_time << " in timeline" << endl;

}
