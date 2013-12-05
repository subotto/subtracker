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
		
		Node(Blob blob, int time)
			: blob(blob), badness(INFTY), time(time), previous(NULL)
		{};
	
};

class BlobsTracker {
	public:
		void InsertFrameInTimeline(std::vector<Blob> blobs, int time);
		cv::Point2f ProcessFrame(int initial_time, int processed_time);
		void PopFrameFromTimeline();
		
		BlobsTracker()
			: _max_speed(10.0), _max_unseen_distance(50.0)
		{};
	
	private:
		std::deque< std::vector<Node> > _timeline;
		double _max_speed; // massimo spostamento tra due frame consecutivi
		double _max_unseen_distance; // massima distanza a cui si può teletrasportare la pallina

};

#endif
