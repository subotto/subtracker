#include "blobs_tracker.hpp"

#include "blobs_finder.hpp"

#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <cstdio>
#include <iostream>

using namespace cv;
using namespace std;

void BlobsTracker::InsertFrameInTimeline(vector<Blob> blobs, int time) {
	vector<Node> v;
	for (int i=0; i<blobs.size(); i++) {
		Node n = Node( blobs[i], time );
		v.push_back(n);
	}
	_timeline.push_back(v);
}

Point2f BlobsTracker::ProcessFrame(int initial_time, int processed_time) {
	
	printf("Processing frame %d\n", processed_time);
	
	double min_badness = INFTY;
	Node *best_node = NULL;
	
	for (int i=0; i<_timeline.size(); i++) {
		for (int k=0; k<_timeline[i].size(); k++) {
			// Passo base (collegamenti con il nodo fittizio iniziale)
			_timeline[i][k].badness = i;
			_timeline[i][k].previous = NULL;
			
			// Programmazione dinamica
			for (int j=0; j<i; j++) {
				for (int h=0; h<_timeline[j].size(); h++) {
					
					int interval = i-j;
					double old_badness = _timeline[j][h].badness;
					double new_badness = old_badness + ( interval - 1 );
					
					// Controllo di località spaziale --- forse Kalman lo rendera' obsoleto
					Point2f new_center = _timeline[i][k].blob.center;
					Point2f old_center = _timeline[j][h].blob.center;
					if ( norm(new_center-old_center) > _max_speed * interval ) continue;
					
					if ( norm(new_center-old_center) > _max_unseen_distance ) continue;
					// Fine controllo di località spaziale
					
					if ( new_badness < _timeline[i][k].badness ) {
						_timeline[i][k].badness = new_badness;
						// Salvo il percorso ottimo
						_timeline[i][k].previous = &_timeline[j][h];
					}
				}
			}
			
			// Passo finale (collegamenti con il nodo fittizio finale)
			double candidate_badness = _timeline[i][k].badness + ( _timeline.size() - i - 1 );
			if ( min_badness > candidate_badness ) {
				min_badness = candidate_badness;
				best_node = &_timeline[i][k];
			}
		}
	}
	
	Point2f NISBA (-1.0, -1.0);
	
	// Cerco la miglior posizione prevista per il frame richiesto
	Node *node = best_node;
	if ( node == NULL || node->time < processed_time ) return NISBA;
	
	Node *greater = NULL;
	Node *lower = NULL;
	
	while ( node != NULL ) {
		int time = node->time;
		if ( time >= processed_time ) {
			greater = node;
		}
		if ( time <= processed_time ) {
			lower = node;
			break;
		}
		node = node->previous;
	}
	
	if ( lower == NULL ) return NISBA;
	
	// Waiting for great Kalman (Hail, Kalman! Hail!)
	
	if ( greater->time == lower->time ) return greater->blob.center;
	cv::Point2f greater_center = greater->blob.center;
	cv::Point2f lower_center = lower->blob.center;
	
	return ( greater_center*(processed_time - lower->time) + lower_center*(greater->time - processed_time) ) * ( 1.0/(greater->time - lower->time) );
	
	
	/*
	vector<Node> &last = _timeline.back();
	
	if ( last.empty() ) {
		// Nessun blob trovato nel frame corrente
		return Node( Blob( Point2f(0.0,0.0), 0.0, 0.0 ), 0 );
	}
	
	
	for (int k=0; k < last.size(); k++) {
		
		printf("Node %d (time=%d).", k, last[k].time);
		
		// Passo base
		last[k].badness = last[k].time - initial_time;
		last[k].previous = NULL;
		
		for (deque< vector<Node> >::iterator i=_timeline.begin(); i!=_timeline.end(); i++) {
			
			// Controllo di non aver raggiunto il frame corrente
			deque< vector<Node> >::iterator ii=i;
			ii++;
			if ( ii == _timeline.end() ) break;
			
			// Scorro i blob del frame *i
			vector<Node> &old = *i;
			
			for (int j=0; j < i->size(); j++) {
				
				int interval = last[k].time - old[j].time;
				double old_badness = old[j].badness;
				
				double new_badness = old_badness + ( interval - 1 );
				
				
				// Controllo di località spaziale --- forse Kalman lo rende obsoleto
				Point2f new_center = last[k].blob.center;
				Point2f old_center = old[j].blob.center;
				double max_speed = 10.0; // massimo spostamento tra due frame consecutivi
				if ( norm(new_center-old_center) > max_speed * interval ) continue;
				// Fine controllo di località spaziale
				
				
				if ( new_badness < last[k].badness ) {
					last[k].badness = new_badness;
					// Salvare il percorso ottimo
					last[k].previous = &old[j];
				}
				
			}
		}
		
		printf("Badness = %lf\n", last[k].badness);
	}
	
	
	
	
	double min_badness = INFTY;
	int best_node_index = 0;
	
	for (int k=0; k < last.size(); k++) {
		if ( min_badness > last[k].badness ) {
			
			best_node_index = k;
			min_badness = last[k].badness;
			
		}
	}
	
	printf("Scelgo il nodo %d\n", best_node_index);
	
	return last[best_node_index];
	*/
}

void BlobsTracker::PopFrameFromTimeline() {
	_timeline.pop_front();
}

