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

  BlobsTracker(control_panel_t& panel);

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
  double sigma0;
  double sigma_per_second;
private:
  std::deque< std::vector<Node> > _timeline;
  control_panel_t& panel;
};

#endif
