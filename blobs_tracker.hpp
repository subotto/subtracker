#ifndef __BLOBS_TRACKER_H_
#define __BLOBS_TRACKER_H_

#include "control.hpp"

#include <vector>
#include <deque>

double const INFTY = 1e100;


class Blob {
	public:
		cv::Point2f center;
		double radius;
		double speed;
		double weight;
	
		Blob(cv::Point2f center, double radius=0, double speed=0, double weight=0)
			: center(center), radius(radius), speed(speed), weight(weight)
		{};
		
	private:
	
		
};

class Node {
	
	public:
		Blob blob;
		double badness;
		int time;
		Node* previous;
		Node* next;
		bool is_absent;	// The ball isn't on the field
		
		Node(Blob blob, int time, bool is_absent)
			: blob(blob), badness(INFTY), time(time), previous(NULL), next(NULL), is_absent(is_absent)
		{};
	
};

class BlobsTracker {
	public:
		void InsertFrameInTimeline(std::vector<Blob> blobs, int time);
		std::vector<cv::Point2f> ProcessFrames(int initial_time, int begin_time, int end_time);
		void PopFrameFromTimeline();
		int GetFrontTime();

		
		BlobsTracker(control_panel_t& panel)
			:
				_fps(120.0),
				_max_speed(18.0),
				_max_unseen_distance(0.3),
				_max_interpolation_time(0.5),
				_skip_parameter(-8),
				_variance_parameter(0.3),
				_absent_parameter(-10),
				_appearance_parameter(400.0),
				_disappearance_parameter(400.0),
				panel(panel)
		{
			trackbar(panel, "ball tracking", "fps", _fps, {0, 100, 1});
			trackbar(panel, "ball tracking", "max speed", _max_speed, {0., +100., 0.1});
			trackbar(panel, "ball tracking", "max_unseen_distance", _max_unseen_distance, {0., 1., 0.01});
			trackbar(panel, "ball tracking", "max_interpolation_time", _max_interpolation_time, {0., 10., 0.01});

			trackbar(panel, "ball tracking", "skip parameter", _skip_parameter, {-100., +100., 0.1});
			trackbar(panel, "ball tracking", "variance parameter", _variance_parameter, {0., 10., 0.01});
			trackbar(panel, "ball tracking", "absent parameter", _absent_parameter, {-100., +100., 0.1});
			trackbar(panel, "ball tracking", "appearance parameter", _appearance_parameter, {-1000., +1000., 0.1});
			trackbar(panel, "ball tracking", "disappearance parameter", _disappearance_parameter, {-1000., +1000., 0.1});

			trackbar(panel, "ball tracking", "sigma0", sigma0, {0, 10., 0.01});
			trackbar(panel, "ball tracking", "sigma per second", sigma_per_second, {0, 10., 0.01});
		};
	
		double _fps; // fotogrammi al secondo
		double _max_speed; // massima velocita' (in m/s) perché il movimento sia considerato possibile
		double _max_unseen_distance; // massima distanza (in metri) a cui si può teletrasportare la pallina
		double _max_interpolation_time; // tempo massimo (in secondi) entro il quale si interpola la posizione della pallina
		
		double _skip_parameter;	// costo (in badness) per saltare un frame
		double _variance_parameter; // varianza di in un secondo (Massimo sa cosa significa)
		double _absent_parameter; // costo (in badness) passare da uno stato assente a uno stato assente
		double _appearance_parameter; // costo (in badness) per passare da uno stato assente a uno stato presente
		double _disappearance_parameter; // costo (in badness) per passare da uno stato presente a uno stato assente

		// smoothing
		double sigma0 = 0.03;
		double sigma_per_second = 1.0;
	private:
		std::deque< std::vector<Node> > _timeline;
		control_panel_t& panel;

};

#endif
