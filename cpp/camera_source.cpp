
#include <csignal>
#include <arpa/inet.h>
#include <turbojpeg.h>

#include "framereader.hpp"

volatile bool stop = false;

void interrupt_handler(int param) {
  cerr << "Interrupt received, stopping..." << endl;
  stop = true;
}

typedef struct {
  unsigned char *buf;
  int width, height;
  double timestamp;
} Image;

#define JPEG_QUALITY 95

void write_frame(FILE *fout, const Image &image) {

  unsigned long length;
  unsigned char *buf = NULL;

  tjhandle jpeg_enc = tjInitCompress();
  tjCompress2(jpeg_enc, image.buf, image.width, 0, image.height, TJPF_BGR, &buf, &length, TJSAMP_444, JPEG_QUALITY, TJFLAG_FASTDCT);
  tjDestroy(jpeg_enc);

  uint32_t length32 = htonl((uint32_t) length);
  size_t res;
  res = fwrite(&image.timestamp, 8, 1, fout);
  assert(res == 1);
  res = fwrite(&length32, 4, 1, fout);
  assert(res == 1);
  res = fwrite(buf, 1, length, fout);
  assert(res == length);

  tjFree(buf);

}

int main(int argc, char **argv) {

  if (argc != 5) {
    cerr << "Usage: " << argv[0] << " <V4L2 input num> <width> <height> <fps>" << endl;
    exit(1);
  }

  int v4l2_dev_num = atoi(argv[1]);
  int width = atoi(argv[2]);
  int height = atoi(argv[3]);
  int fps = atoi(argv[4]);

  control_panel_t panel;
  init_control_panel(panel);
  set_log_level(panel, "frames", VERBOSE);

  signal(SIGINT, interrupt_handler);

  FrameReader frame_reader(v4l2_dev_num, panel, width, height, fps);

  int frame_num;
  double prev_time = 0.0;
  for (frame_num = 0; !stop; frame_num++) {
    auto frame_info = frame_reader.get();
    double time = duration_cast<duration<double>>(frame_info.timestamp.time_since_epoch()).count();
    logger(panel, "frames", VERBOSE) << "Received frame number " << frame_num << " with timestamp "
                                     << time
                                     << " and playback time "
                                     << duration_cast<duration<double>>(frame_info.playback_time.time_since_epoch()).count()
                                     << " (time difference: " << time - prev_time << "; reciprocal: " << 1.0 / (time - prev_time) << ")"
                                     << endl;
    assert(frame_info.data.isContinuous());
    assert(frame_info.data.channels() == 3);
    Image image;
    image.buf = frame_info.data.data;
    image.width = frame_info.data.size().width;
    image.height = frame_info.data.size().height;
    image.timestamp = duration_cast<duration<double>>(frame_info.playback_time.time_since_epoch()).count();
    write_frame(stdout, image);
    prev_time = time;
  }

  return 0;

}
