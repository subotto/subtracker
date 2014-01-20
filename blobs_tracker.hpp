#ifndef __BLOBS_TRACKER_H_
#define __BLOBS_TRACKER_H_

#include "blobs_finder.hpp"

#include <vector>
#include <deque>

double const INFTY = 1e100;

/*
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
*/

class Node {
	
	public:
		Blob blob;
		double badness;
		int time;
		Node* previous;
		
		// std::vector<Subnode> subnodes;
		
		Node(Blob blob, int time)
			: blob(blob), badness(INFTY), time(time), previous(NULL)
		{};
	
};

class BlobsTracker {
	public:
		void InsertFrameInTimeline(std::vector<Blob> blobs, int time);
		std::vector<cv::Point2f> ProcessFrames(int initial_time, int begin_time, int end_time, bool debug);
		void PopFrameFromTimeline();
		
		BlobsTracker()
			: _fps(120.0), _max_speed(18.0), _max_unseen_distance(0.3), _distance_constant(1000.0), _max_interpolation_time(0.5), _max_badness_per_frame(-7.0)
		{};
	
	private:
		std::deque< std::vector<Node> > _timeline;
		double _fps; // fotogrammi al secondo
		double _max_speed; // massima velocita' (in m/s) perché il movimento sia considerato possibile
		double _max_unseen_distance; // massima distanza (in metri) a cui si può teletrasportare la pallina
		double _distance_constant; // costante per decidere la verosimiglianza della pallina spostata
		double _max_interpolation_time; // tempo massimo (in secondi) entro il quale si interpola la posizione della pallina
		double _max_badness_per_frame; // costante per evitare soluzioni con badness troppo alta
};

#endif
