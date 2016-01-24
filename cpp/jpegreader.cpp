
#include "jpegreader.hpp"

#include <arpa/inet.h>

FrameFromFile::FrameFromFile(istream &fin, control_panel_t &panel)
  : FrameCycle(panel), fin(fin), tj_dec(tjInitDecompress())  {
}

FrameFromFile::~FrameFromFile() {
  tjDestroy(this->tj_dec);
}

bool FrameFromFile::process_frame() {

  // Read from stream
  double timestamp;
  uint32_t length;
  this->fin.read((char*) &timestamp, sizeof(double));
  if (this->fin.gcount() != sizeof(double)) {
    return false;
  }
  this->fin.read((char*) &length, sizeof(uint32_t));
  if (this->fin.gcount() != sizeof(uint32_t)) {
    return false;
  }
  length = ntohl(length);
  // See http://stackoverflow.com/a/30605295/807307
  vector< char > buffer(length);
  this->fin.read(&buffer[0], length);
  if (this->fin.gcount() != length) {
    return false;
  }

  // Actually decode image
  FrameInfo info;
  int width, height, subsamp, res;
  res = tjDecompressHeader2(this->tj_dec, (unsigned char*) &buffer[0], length, &width, &height, &subsamp);
  if (!res) {
    return false;
  }
  info.data = Mat(height, width, CV_8UC3);
  assert(info.data.elemSize() == 3);
  res = tjDecompress2(this->tj_dec, (unsigned char*) &buffer[0], length, info.data.data, width, info.data.step[0], height, TJPF_BGR, TJFLAG_ACCURATEDCT);
  if (!res) {
    return false;
  }

  // Fill other satellite information and send frame
  info.valid = true;
  info.playback_time = time_point< system_clock >(duration_cast< time_point< system_clock >::duration >(duration< double >(timestamp)));
  if (!this->first_frame_seen) {
    this->first_frame_seen = true;
    this->first_frame_time = info.playback_time;
  }
  info.timestamp = time_point< video_clock >(duration_cast< time_point< video_clock >::duration >(info.playback_time - this->first_frame_time));
  this->push(info);

}
