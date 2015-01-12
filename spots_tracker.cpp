
#include <algorithm>

#include "spots_tracker.hpp"

const double INFTY = 1e100;

Spot::Spot() {

}

Spot::Spot(Point2f center, double weight)
  : center(center), weight(weight) {

}

SpotNode::SpotNode(Spot spot, double time, int num, bool present)
  : spot(spot), time(time), num(num), present(present), badness(INFTY), prev(NULL) {

}

SpotsTracker::SpotsTracker(control_panel_t &panel)
  : panel(panel),
    node_num(0),
    front_num(-1),

    dynamic_depth(60),
    back_badness(INFTY),
    appearance_badness(400.0),
    disappearance_badness(400.0),
    absence_badness(-10.0),
    max_speed(18.0),
    max_unseen_distance(0.3),
    skip_badness(-8.0),
    variance_parameter(0.3) {

  // Push a frame with just an absent node; it will be ignored when
  // returning results
  SpotNode phantom_node(Spot({0.0, 0.0}, 0.0), 0.0, -1, false);
  phantom_node.badness = 0.0;
  vector< SpotNode > v;
  v.push_back(phantom_node);

  this->frame_num_rev_map[-1] = 0.0;
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

  // Remember the reverse map from frame_num to time
  this->frame_num_rev_map[this->node_num] = time;

  // When first frame is pushed, put its timestamp to the initial
  // phantom frame (so that time counte does not explode)
  if (this->timeline.front().front().num == -1) {
    this->timeline.front().front().time = time;
  }

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
    for (long int i = begin; i < this->timeline.size(); i++) {
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
          n2.prev_num = n1.num;
          n2.prev_time = n1.time;
          n2.prev_present = n1.present;
          n2.prev_spot = n1.spot;
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

static Point2f interpolate(double a, double t, double b, Point2f x, Point2f y) {

  double t1 = (t - a) / (b - a);
  return (1 - t1) * x + t1 * y;

}

SpotNode &SpotsTracker::_front() {

  // Go backward until you find the front level
  vector< SpotNode >::iterator it = this->back_best;
  while (it->prev_num > this->front_num) {
    it = it->prev;
  }
  return *it;

}

tuple< bool, Point2f > SpotsTracker::front() {

  // Return _front(), unless it has number -1; in that case try to pop
  if (this->front_num == -1) {
    this->_pop_front();
  }
  SpotNode &ret = this->_front();

  if (ret.prev_num == this->front_num) {
    return make_tuple(ret.prev_present, ret.prev_spot.center);
  } else if (ret.num == this->front_num) {
    return make_tuple(ret.present, ret.spot.center);
  } else {
    if (ret.present && ret.prev_present) {
      return make_tuple(true, interpolate(ret.prev_time, this->frame_num_rev_map[this->front_num], ret.time, ret.prev_spot.center, ret.spot.center));
    } else {
      return make_tuple(false, ret.spot.center);
    }
  }

}

void SpotsTracker::_pop_front() {

  this->timeline.pop_front();
  this->frame_num_rev_map.erase(this->front_num);
  this->front_num += 1;

  assert(this->front_num == this->timeline.front().front().num);

}

void SpotsTracker::pop_front() {

  if (this->front_num == -1) this->_pop_front();
  this->_pop_front();

}

int SpotsTracker::get_front_num() {

  return this->front_num;

}
