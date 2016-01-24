
#include "jpegreader.hpp"

#include <arpa/inet.h>

JPEGReader::JPEGReader(string file_name, control_panel_t &panel, bool from_file, bool simulate_live, int width, int height)
  : FrameCycle(panel), fin(file_name), tj_dec(tjInitDecompress()), width(width), height(height), from_file(from_file) {

  if (from_file) {
    if (simulate_live) {
      this->rate_limited = true;
    } else {
      this->can_drop_frames = false;
    }
  }

  //logger(panel, "jpeg", DEBUG) << "can_drop_frames: " << this->can_drop_frames << "; rate_limited: " << this->rate_limited << endl;

}

JPEGReader::~JPEGReader() {

  tjDestroy(this->tj_dec);

}

bool JPEGReader::process_frame() {

  // Read from stream
  double timestamp;
  uint32_t length;
  this->fin.read((char*) &timestamp, sizeof(double));
  if (this->fin.gcount() != sizeof(double)) {
    logger(panel, "jpeg", ERROR) << "Cannot read timestamp" << endl;
    return false;
  }
  this->fin.read((char*) &length, sizeof(uint32_t));
  if (this->fin.gcount() != sizeof(uint32_t)) {
    logger(panel, "jpeg", ERROR) << "Cannot read length" << endl;
    return false;
  }
  length = ntohl(length);
  // See http://stackoverflow.com/a/30605295/807307
  vector< char > buffer(length);
  this->fin.read(&buffer[0], length);
  if (this->fin.gcount() != length) {
    logger(panel, "jpeg", ERROR) << "Cannot read data" << endl;
    return false;
  }

  // Actually decode image
  FrameInfo info;
  int width, height, subsamp, res;
  res = tjDecompressHeader2(this->tj_dec, (unsigned char*) &buffer[0], length, &width, &height, &subsamp);
  if (res) {
    logger(panel, "jpeg", ERROR) << "Cannot decompress JPEG header" << endl;
    return false;
  }
  if (this->width >= 0) {
    width = this->width;
  }
  if (this->height >= 0) {
    height = this->height;
  }
  info.data = Mat(height, width, CV_8UC3);
  assert(info.data.elemSize() == 3);
  res = tjDecompress2(this->tj_dec, (unsigned char*) &buffer[0], length, info.data.data, width, info.data.step[0], height, TJPF_BGR, TJFLAG_ACCURATEDCT);
  if (res) {
    logger(panel, "jpeg", ERROR) << "Cannot decompress JPEG image" << endl;
    return false;
  }

  // Fill other satellite information and send frame
  info.valid = true;
  info.time = time_point< system_clock >(duration_cast< time_point< system_clock >::duration >(duration< double >(timestamp)));
  if (!this->first_frame_seen) {
    this->first_frame_seen = true;
    this->first_frame_time = info.time;
  }
  if (this->from_file) {
    info.playback_time = this->video_start_playback_time + (info.time - this->first_frame_time);
  } else {
    info.playback_time = info.time;
  }
  this->push(info);

}
