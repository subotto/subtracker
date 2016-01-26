
#include <csignal>
#include <iostream>

#include "jpegreader.hpp"
#include "utility.hpp"

using namespace std;

volatile bool stop = false;

void interrupt_handler(int param) {
  cerr << "Interrupt received, stopping..." << endl;
  stop = true;
}

int main(int argc, char **argv) {

  if (argc != 5) {
    cerr << "Usage: " << argv[0] << " <input> <width> <height> <fps>" << endl;
    exit(1);
  }

  string file_name(argv[1]);
  int width = atoi(argv[2]);
  int height = atoi(argv[3]);
  int fps = atoi(argv[4]);

  control_panel_t panel;
  init_control_panel(panel);
  signal(SIGINT, interrupt_handler);

  auto f = open_frame_cycle(file_name, panel);
  f->start();

  int frame_num;
  for (frame_num = 0; !stop; frame_num++) {
    auto frame_info = f->get();
    assert(frame_info.data.isContinuous());
    assert(frame_info.data.channels() == 3);
    namedWindow("frame");
    imshow("frame", frame_info.data);
    int c = waitKey(1);
    if (c > 0) c &= 0xff;
    if (c == 'q') break;
  }

  delete f;

  return 0;

}
