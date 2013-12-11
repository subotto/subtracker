#ifndef __BLOBS_TRACKER_H_
#define __BLOBS_TRACKER_H_

#include "blobs_finder.hpp"

#include <vector>
#include <deque>

double const INFTY = 1e100;

class Node;

class Subnode {
	
	public:
		double badness;
		cv::Point2f speed;
		Node* previous;
		int previous_subnode;
		
		Subnode(double badness, cv::Point2f speed, Node* previous, int previous_subnode)
			: badness(badness), speed(speed), previous(previous), previous_subnode(previous_subnode)
		{};
	
};

class Node {
	
	public:
		Blob blob;
		double badness;
		int time;
		Node* previous;
		
		std::vector<Subnode> subnodes;
		
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
			: _max_speed(10.0), _max_unseen_distance(50.0), _distance_constant(0.01), _max_interpolation_time(80)
		{};
	
	private:
		std::deque< std::vector<Node> > _timeline;
		double _max_speed; // massimo spostamento tra due frame consecutivi perché il movimento sia riconosciuto possibile
		double _max_unseen_distance; // massima distanza a cui si può teletrasportare la pallina
		double _distance_constant; // costante per decidere la verosimiglianza della pallina spostata
		int _max_interpolation_time; // numero massimo di fotogrammi entro i quali si interpola la posizione della pallina

};

#endif
