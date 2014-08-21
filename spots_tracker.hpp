
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
  Spot *prev;
  bool present;

  SpotNode(Spot spot, double time, bool present);
};

class SpotsTracker {
public:
  void insert(vector< Spot > spot, double time);

  SpotsTracker(control_panel_t &panel);

private:
  deque< vector< SpotNode > > timeline;
  control_panel_t &panel;
};

#endif
