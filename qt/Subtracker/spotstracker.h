#ifndef SPOTSTRACKER_H
#define SPOTSTRACKER_H

#include <vector>
#include <deque>
#include <unordered_map>

#include <opencv2/core/core.hpp>

struct Spot {
  cv::Point2f center;
  double weight;
};

struct SpotNode {
  Spot spot;
  double badness;
  double time;
  int num;
  std::vector< SpotNode >::iterator prev;
  bool present;

  // Backup values of the previous node, so we have them also when
  // previous node gets destroyed
  int prev_num;
  bool prev_present;
  Spot prev_spot;
  double prev_time;

  SpotNode(Spot spot, double time, int num, bool present);
};

class SpotsTracker {
public:
  void push_back(std::vector< Spot > spot, double time);
  std::tuple< bool, cv::Point2f > front();
  void pop_front();
  int get_front_num();

  SpotsTracker();

private:
  std::deque< std::vector< SpotNode > > timeline;
  std::unordered_map< int, double > frame_num_rev_map;
  int node_num;
  int front_num;
  double back_badness;
  std::vector< SpotNode >::iterator back_best;

  int dynamic_depth;
  double appearance_badness;
  double disappearance_badness;
  double absence_badness;
  double max_speed;
  double max_unseen_distance;
  double skip_badness;
  double variance_parameter;

  std::tuple< bool, double > jump_badness(const SpotNode &n1, const SpotNode &n2) const;
  SpotNode &_front();
  void _pop_front();
};


#endif // SPOTSTRACKER_H
