
#include "spots_tracker.hpp"

const double INFTY = 1e100;

Spot::Spot(Point2f center, double weight)
  : center(center), weight(weight) {

}

SpotNode::SpotNode(Spot spot, double time, bool present)
  : spot(spot), time(time), present(present), badness(INFTY), prev(NULL) {

}

SpotsTracker::SpotsTracker(control_panel_t &panel)
  : panel(panel) {

  // Push a frame with just an absent node; it will be ignored when
  // returning results
  SpotNode phantom_node(Spot({0.0, 0.0}, 0.0), 0.0, false);
  phantom_node.badness = 0.0;
  vector< SpotNode > v;
  v.push_back(phantom_node);

  this->timeline.push_back(v);

}

void SpotsTracker::insert(vector< Spot > spots, double time) {

  // Prepere a vector with the new nodes
  vector< SpotNode > v;
  for (auto spot : spots) {
    v.emplace_back(spot, time, true);
  }
  v.emplace_back(Spot({0.0, 0.0}, 0.0), time, false);

  // Implement a step of the dynamic algorithm (with bounded depth)
  // TODO

  // Push the vector
  this->timeline.push_back(v);

}
