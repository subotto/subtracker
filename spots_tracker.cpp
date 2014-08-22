
#include <algorithm>

#include "spots_tracker.hpp"

const double INFTY = 1e100;

Spot::Spot(Point2f center, double weight)
  : center(center), weight(weight) {

}

SpotNode::SpotNode(Spot spot, double time, int num, bool present)
  : spot(spot), time(time), num(num), present(present), badness(INFTY), prev(NULL) {

}

SpotsTracker::SpotsTracker(control_panel_t &panel)
  : panel(panel), front_num(-1), dynamic_depth(60) {

  // Push a frame with just an absent node; it will be ignored when
  // returning results
  SpotNode phantom_node(Spot({0.0, 0.0}, 0.0), 0.0, -1, false);
  phantom_node.badness = 0.0;
  vector< SpotNode > v;
  v.push_back(phantom_node);

  this->timeline.push_back(v);

}

tuple< bool, double > SpotsTracker::jump_badness(const SpotNode &n1, const SpotNode &n2) const {

  double ret = 0.0;
  double time = n2.time - n1.time;
  double skip = n2.num - n1.num - 1;
  assert(time >= 0);
  assert(skip >= 0);

  // Presence transitions
  if (n1.present && !n2.present) {
    // Present to absent
    ret += this->disappearance_badness;
  } else if (!n1.present && n2.present) {
    // Absent to present
    ret += this->appearance_badness;
  } else if (!n1.present && !n2.present) {
    // Absent to absent
    ret += this->absence_badness;
    if (skip > 0) return make_tuple(false, 0.0);
  }

  // Node weight
  if (n2.present) {
    ret -= n2.spot.weight;
  }

  // Skip badness
  ret += skip * this->skip_badness;

  // Locality check and Gaussian likelihood
  if (n1.present && n2.present) {
    double distance = norm(n1.spot.center - n2.spot.center);
    if (distance > this->max_speed * time) return make_tuple(false, 0.0);
    if (distance > this->max_unseen_distance) return make_tuple(false, 0.0);
    ret += distance * distance / (time * this->variance_parameter);
  }

  return make_tuple(true, ret);

}

void SpotsTracker::push_back(vector< Spot > spots, double time) {

  // Prepere a vector with the new nodes
  vector< SpotNode > v;
  for (auto spot : spots) {
    v.emplace_back(spot, time, this->node_num, true);
  }
  v.emplace_back(Spot({0.0, 0.0}, 0.0), time, this->node_num, false);
  this->node_num++;

  long int begin = std::max< long int >(0, this->timeline.size() - this->dynamic_depth);
  this->back_badness = INFTY;
  for (vector< SpotNode >::iterator it2 = v.begin(); it2 != v.end(); it2++) {
    SpotNode &n2 = *it2;

    // Implement a step of the dynamic programming algorithm (with
    // bounded depth)
    for (long int i = begin; i <= this->timeline.size(); i++) {
      for (vector< SpotNode >::iterator it1 = this->timeline[i].begin(); it1 != this->timeline[i].end(); it1++) {
        SpotNode &n1 = *it1;
        bool legal_jump;
        double delta_badness;
        tie(legal_jump, delta_badness) = this->jump_badness(n1, n2);
        if (!legal_jump) continue;
        double new_badness = n1.badness + delta_badness;
        if (new_badness < n2.badness) {
          n2.badness = new_badness;
          n2.prev = it1;
        }
      }
    }

    // Update the best position in the back of the queue
    if (n2.badness < this->back_badness) {
      this->back_badness = n2.badness;
      this->back_best = it2;
    }
  }

  // Push the vector
  this->timeline.push_back(v);

}

SpotNode &SpotsTracker::_front() {

  // Go backward until you find the front level
  vector< SpotNode >::iterator it = this->back_best;
  while (it->num != this->front_num) {
    it = it->prev;
  }
  return *it;

}

SpotNode SpotsTracker::front() {

  // Return _front(), unless it has number -1; in that case try to pop
  SpotNode &ret = this->_front();
  if (ret.num == -1) {
    this->pop_front();
    ret = this->_front();
  }
  return ret;

}

void SpotsTracker::pop_front() {

  this->timeline.pop_front();
  this->front_num += 1;

  assert(this->front_num == this->timeline.front().front().num);

}
