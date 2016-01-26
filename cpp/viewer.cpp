
#include <csignal>
#include <iostream>

#include "jpegreader.hpp"
#include "utility.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>

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

  Size image_size(640, 480);

  auto f = (JPEGReader*) open_frame_cycle(file_name, panel, image_size.width, image_size.height);
  f->set_droppy(true);
  f->start();

  // Load camera distortion parameters
  FileStorage fs("camera_parameters", FileStorage::READ);
  Mat camera_matrix, distortion_coefficients;
  fs["camera_matrix"] >> camera_matrix;
  fs["distortion_coefficients"] >> distortion_coefficients;
  Mat new_camera_matrix = getOptimalNewCameraMatrix(camera_matrix, distortion_coefficients, image_size, 1.0, image_size, NULL, false);
  Mat undistort_map1, undistort_map2;
  initUndistortRectifyMap(camera_matrix, distortion_coefficients, Mat(), new_camera_matrix, image_size, CV_16SC2, undistort_map1, undistort_map2);

  int frame_num;
  for (frame_num = 0; !stop; frame_num++) {
    auto frame_info = f->get();
    assert(frame_info.data.isContinuous());
    assert(frame_info.data.channels() == 3);
    Mat undistorted_frame;
    remap(frame_info.data, undistorted_frame, undistort_map1, undistort_map2, INTER_LINEAR);
    namedWindow("frame");
    namedWindow("undistorted frame");
    imshow("frame", frame_info.data);
    imshow("undistorted frame", undistorted_frame);
    int c = waitKey(100);
    if (c > 0) c &= 0xff;
    if (c == 'q') break;
  }

  delete f;

  return 0;

}
