
#ifndef _SPOTS_TRACKER_H
#define _SPOTS_TRACKER_H

#include "control.hpp"

#include <vector>
#include <deque>

using namespace std;
using namespace cv;

class Spot {
public:
  Point2f center;
  double weight;

public:
  Spot(Point2f center, double weight);
};

class SpotNode {
public:
  Spot spot;
  double badness;
  double time;
  int num;
  vector< SpotNode >::iterator prev;
  bool present;

  SpotNode(Spot spot, double time, int num, bool present);
};

class SpotsTracker {
public:
  void push_back(vector< Spot > spot, double time);
  SpotNode front();
  void pop_front();

  SpotsTracker(control_panel_t &panel);

private:
  deque< vector< SpotNode > > timeline;
  control_panel_t &panel;
  int node_num;
  int front_num;
  double back_badness;
  vector< SpotNode >::iterator back_best;

  int dynamic_depth;
  double appearance_badness;
  double disappearance_badness;
  double absence_badness;
  double max_speed;
  double max_unseen_distance;
  double skip_badness;
  double variance_parameter;

  tuple< bool, double > jump_badness(const SpotNode &n1, const SpotNode &n2) const;
  SpotNode &_front();
};

#endif
