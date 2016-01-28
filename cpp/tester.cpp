#include <iostream>
#include <iterator>
#include <string>
#include <deque>

#include <cmath>
#include <chrono>

#include "context.hpp"

using namespace cv;
using namespace std;
using namespace chrono;

control_panel_t panel;

int main(int argc, char* argv[]) {

  Mat first(320, 240, .0f), second(320, 240, .0f);
  FrameSettings fs(first, second);
  auto time = system_clock::now();
  //FrameAnalysis fa(Mat(), 0, system_clock::now(), FrameSettings(Mat(), Mat()), panel);
  for (int i = 0; i < 1; i++) {
    FrameAnalysis fa(Mat(), 0, time, fs, panel);
  }
  auto later = system_clock::now();

  cout << "Time difference: " << (duration_cast< duration< double > >(later-time)).count() << endl;

  return 0;

}
