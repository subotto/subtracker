#include "blobs_tracker.hpp"

#include "blobs_finder.hpp"

#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <cstdio>
#include <iostream>
#include <algorithm>
#include <vector>

using namespace cv;
using namespace std;

/*
bool operator< (Subnode const &a, Subnode const &b) {
	return ( a.badness < b.badness );
}
*/

void BlobsTracker::InsertFrameInTimeline(vector<Blob> blobs, int time) {
	vector<Node> v;
	for (int i=0; i<blobs.size(); i++) {
		Node n = Node( blobs[i], time );
		v.push_back(n);
	}
	_timeline.push_back(v);
}


vector<Point2f> BlobsTracker::ProcessFrames(int initial_time, int begin_time, int end_time, bool debug) {
	
	if (debug) printf("Processing frames from %d to %d\n", begin_time, end_time-1);
	
	 // questa min_badness serve a evitare falsi positivi quando non c'è la pallina vera
	double min_badness = _timeline.size() * _max_badness_per_frame;
	if (debug) printf("Maximum acceptable badness: %.1lf\n", min_badness);
	Node *best_node = NULL;
	
	for (int i=0; i<_timeline.size(); i++) {
		for (int k=0; k<_timeline[i].size(); k++) {
			
			// Passo base (collegamenti con il nodo fittizio iniziale)
			_timeline[i][k].badness = 0;
			_timeline[i][k].previous = NULL;
			
			
			// Programmazione dinamica
			for (int j=0; j<i; j++) {
				for (int h=0; h<_timeline[j].size(); h++) {
					
					double old_badness = _timeline[j][h].badness;
					int interval = i-j;
					
					double delta_badness = - _timeline[j][h].blob.weight;
					
					// Località spaziale
					Point2f new_center = _timeline[i][k].blob.center;
					Point2f old_center = _timeline[j][h].blob.center;
					double distance = norm(new_center - old_center);
					
					// Controllo di località
					if ( distance > _max_speed / _fps * interval ) continue;
					if ( distance > _max_unseen_distance ) continue;
					
					// Verosimiglianza gaussiana
					delta_badness += _distance_constant * distance * distance;
					
					// Calcolo la nuova badness, e aggiorno se è minore della minima finora trovata
					double new_badness = old_badness + delta_badness;
					
					if ( new_badness < _timeline[i][k].badness ) {
						_timeline[i][k].badness = new_badness;
						// Salvo il percorso ottimo
						_timeline[i][k].previous = &_timeline[j][h];
					}
					
				}
			}
			
			// Passo finale (collegamenti con il nodo fittizio finale)
			double candidate_badness = _timeline[i][k].badness;
			if ( min_badness > candidate_badness ) {
				min_badness = candidate_badness;
				best_node = &_timeline[i][k];
			}
			
		}
	}
	
	Point2f NISBA(1000.0,1000.0);
	vector<Point2f> positions;
	Point2f position;
	
	if (debug) printf("Badness: %lf\n", min_badness);
	for (int i=begin_time; i<end_time; i++) {
		
		// Cerco la miglior posizione prevista per il frame i
		Node *node = best_node;
		if ( node == NULL ) {
			if (debug) printf("Frame %d: no ball found (no path with acceptable badness).\n", i);
			positions.push_back(NISBA);
			continue;
		}
		if ( node->time < i ) {
			if (debug) printf("Frame %d: no ball found (path does not pass through current frame).\n", i);
			positions.push_back(NISBA);
			continue;
		}
	
		Node *greater = NULL;
		Node *lower = NULL;
	
		while ( node != NULL ) {
			int time = node->time;
			if ( time >= i ) {
				greater = node;
			}
			if ( time <= i ) {
				lower = node;
				break;
			}
			node = node->previous;
		}
	
		if ( lower == NULL ) {
			if (debug) printf("Frame %d: no ball found (path does not pass through current frame).\n", i);
			positions.push_back(NISBA);
			continue;
		}
		
	
		if ( greater->time == lower->time ) {
			if (debug) printf("Frame %d: ball found (exact location). ", i);
			position = greater->blob.center;
			if (debug) printf("Position: (%lf, %lf)\n", position.x, position.y);
			positions.push_back( Point2f(position) );
			continue;
		}
	
		// Se è richiesta la posizione in un frame molto lontano da quelli in cui passa il percorso, restituisco NISBA.
		int pre_diff = i - lower->time;
		int post_diff = greater->time - i;
	
		if ( (double)(max( pre_diff, post_diff )) > _max_interpolation_time * _fps ) {
			if (debug) printf("Frame %d: no ball found (interpolation is not reliable).\n", i);
			positions.push_back(NISBA);
			continue;
		}
	
	
		cv::Point2f greater_center = greater->blob.center;
		cv::Point2f lower_center = lower->blob.center;
	
		if (debug) printf("Frame %d: ball found (estimated location). ", i);
		position = ( greater_center*(i - lower->time) + lower_center*(greater->time - i) ) * ( 1.0/(greater->time - lower->time) );
		if (debug) printf("Position: (%lf, %lf)\n", position.x, position.y);
		positions.push_back( Point2f(position) );
	}
	
	return positions;
}


/* OLD VERSION

Point2f BlobsTracker::ProcessFrame(int initial_time, int processed_time) {
	
	printf("Processing frame %d\n", processed_time);
	
	 // TODO: aggiustare questa min_badness, che serve a evitare falsi positivi quando non c'è la pallina vera
	double min_badness = (double)(_timeline.size()) / 1.0;
	printf("Maximum acceptable badness: %.1lf\n", min_badness);
	Node *best_node = NULL;
	
	for (int i=0; i<_timeline.size(); i++) {
		for (int k=0; k<_timeline[i].size(); k++) {
			
			// Passo base (collegamenti con il nodo fittizio iniziale)
			_timeline[i][k].badness = i;
			_timeline[i][k].previous = NULL;
			
			// _timeline[i][k].subnodes.clear();
			
			
			// Programmazione dinamica
			for (int j=0; j<i; j++) {
				for (int h=0; h<_timeline[j].size(); h++) {
					
					int interval = i-j;
					double old_badness = _timeline[j][h].badness;
					double delta_badness = interval - 1;
					
					// Località spaziale --- forse Kalman la renderà obsoleta
					Point2f new_center = _timeline[i][k].blob.center;
					Point2f old_center = _timeline[j][h].blob.center;
					double distance = norm(new_center - old_center);
					
					// Controllo di località
					if ( distance > _max_speed * interval ) continue;
					// if ( distance > _max_unseen_distance ) continue;
					
					// Verosimiglianza gaussiana
					delta_badness += _distance_constant * distance * distance;
					
					
					// Calcolo la nuova badness, e aggiorno se è minore della minima finora trovata
					double new_badness = old_badness + delta_badness;
					
					if ( new_badness < _timeline[i][k].badness ) {
						_timeline[i][k].badness = new_badness;
						// Salvo il percorso ottimo
						_timeline[i][k].previous = &_timeline[j][h];
					}
					
				}
			}
			
			// Passo finale (collegamenti con il nodo fittizio finale)
			// TODO: controllare che questa cosa sia effettivamente al posto giusto
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
	if ( node == NULL ) {
		printf("No ball found (no path).\n");
		return NISBA;
	}
	if ( node->time < processed_time ) {
		printf("No ball found (path does not pass through current frame).\n");
		return NISBA;
	}
	
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
	
	if ( lower == NULL ) {
		printf("No ball found (path does not pass through current frame).\n");
		return NISBA;
	}
	
	// Waiting for great Kalman (Hail, Kalman! Hail!)
	
	if ( greater->time == lower->time ) {
		printf("Ball found (exact location). Badness: %.1lf\n", min_badness);
		return greater->blob.center;
	}
	
	// Se è richiesta la posizione in un frame molto lontano da quelli in cui passa il percorso, restituisco NISBA.
	int pre_diff = processed_time - lower->time;
	int post_diff = greater->time - processed_time;
	
	if ( max( pre_diff, post_diff ) > _max_interpolation_time ) {
		printf("No ball found (interpolation is not reliable).\n");
		return NISBA;
	}
	
	
	cv::Point2f greater_center = greater->blob.center;
	cv::Point2f lower_center = lower->blob.center;
	
	printf("Ball found (estimated location). Badness: %.1lf\n", min_badness);
	return ( greater_center*(processed_time - lower->time) + lower_center*(greater->time - processed_time) ) * ( 1.0/(greater->time - lower->time) );
	
}
*/

void BlobsTracker::PopFrameFromTimeline() {
	_timeline.pop_front();
}

