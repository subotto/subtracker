
#include <csignal>
#include <iostream>

#include "jpegreader.hpp"
#include "utility.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/xfeatures2d.hpp>

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
  toggle(panel, "timing", TIME);
  set_log_level(panel, "capture", INFO);
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
  Mat new_camera_matrix = getOptimalNewCameraMatrix(camera_matrix, distortion_coefficients, image_size, 0.0, image_size, NULL, true);
  Mat undistort_map1, undistort_map2;
  initUndistortRectifyMap(camera_matrix, distortion_coefficients, Mat(), new_camera_matrix, image_size, CV_16SC2, undistort_map1, undistort_map2);

  // Create SIFT detector
  auto detector = BRISK::create();

  int frame_num;
  for (frame_num = 0; !stop; frame_num++) {
    auto frame_info = f->get();
    dump_time(panel, "timing", "get frame");
    if (!frame_info.valid) {
      break;
    }
    assert(frame_info.data.isContinuous());
    assert(frame_info.data.channels() == 3);
    namedWindow("frame");
    imshow("frame", frame_info.data);
    dump_time(panel, "timing", "show frame");

    /*Mat undistorted_frame;
    remap(frame_info.data, undistorted_frame, undistort_map1, undistort_map2, INTER_LINEAR);
    namedWindow("undistorted frame");
    imshow("undistorted frame", undistorted_frame);
    dump_time(panel, "timing", "undistort frame");*/

    Mat gray_frame;
    cvtColor(frame_info.data, gray_frame, COLOR_BGR2GRAY);
    dump_time(panel, "timing", "gray conversion");

    vector< KeyPoint > keypoints;
    detector->detect(gray_frame, keypoints);
    dump_time(panel, "timing", "detect keypoints");

    Mat keypoints_frame;
    drawKeypoints(gray_frame, keypoints, keypoints_frame, Scalar::all(-1), DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    dump_time(panel, "timing", "draw keypoints");

    namedWindow("keypoints frame");
    imshow("keypoints frame", keypoints_frame);
    dump_time(panel, "timing", "show keypoints frame");

    int c = waitKey(1);
    if (c > 0) c &= 0xff;
    dump_time(panel, "timing", "gui");
    if (c == 'q') break;
  }

  delete f;

  return 0;

}
