#ifndef __BLOBS_TRACKER_H_
#define __BLOBS_TRACKER_H_

#include "blobs_finder.hpp"

#include <vector>
#include <deque>

double const INFTY = 1e100;


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
		std::vector<cv::Point2f> ProcessFrames(int initial_time, int begin_time, int end_time, bool debug);
		void PopFrameFromTimeline();
		
		BlobsTracker()
			: _fps(120.0), _max_speed(18.0), _max_unseen_distance(0.3), _max_interpolation_time(0.5), _skip_parameter(30.0), _distance_parameter(500.0), _appearance_parameter(300.0), _disappearance_parameter(10.0)
		{};
	
	private:
		std::deque< std::vector<Node> > _timeline;
		double _fps; // fotogrammi al secondo
		double _max_speed; // massima velocita' (in m/s) perché il movimento sia considerato possibile
		double _max_unseen_distance; // massima distanza (in metri) a cui si può teletrasportare la pallina
		double _max_interpolation_time; // tempo massimo (in secondi) entro il quale si interpola la posizione della pallina
		
		double _skip_parameter;	// costo (in badness) per saltare un frame
		double _distance_parameter; // costante per decidere la verosimiglianza della pallina spostata
		double _appearance_parameter; // costo (in badness) per passare da uno stato assente a uno stato presente
		double _disappearance_parameter; // costo (in badness) per passare da uno stato presente a uno stato assente
};

#endif
