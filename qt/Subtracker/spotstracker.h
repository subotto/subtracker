#ifndef SPOTSTRACKER_H
#define SPOTSTRACKER_H

#include <vector>
#include <deque>
#include <unordered_map>

#include <opencv2/core/core.hpp>

struct Spot {
  cv::Point2f center;
  float weight;
};

struct SpotNode {
  Spot spot;
  float badness;
  float time;
  int num;
  std::vector< SpotNode >::iterator prev;
  bool present;

  // Backup values of the previous node, so we have them also when
  // previous node gets destroyed
  int prev_num;
  bool prev_present;
  Spot prev_spot;
  float prev_time;

  SpotNode(Spot spot, float time, int num, bool present);
};

class SpotsTracker {
public:
  void push_back(std::vector< Spot > spot, float time);
  std::tuple< bool, cv::Point2f > front();
  void pop_front();
  int get_front_num();

  SpotsTracker();

private:
  std::deque< std::vector< SpotNode > > timeline;
  std::unordered_map< int, float > frame_num_rev_map;
  int node_num;
  int front_num;
  float back_badness;
  std::vector< SpotNode >::iterator back_best;

  int dynamic_depth;
  float appearance_badness;
  float disappearance_badness;
  float absence_badness;
  float max_speed;
  float max_unseen_distance;
  float skip_badness;
  float variance_parameter;

  std::tuple< bool, float > jump_badness(const SpotNode &n1, const SpotNode &n2) const;
  SpotNode &_front();
  void _pop_front();
};


#endif // SPOTSTRACKER_H
