
#include "jpegreader.hpp"

#include <arpa/inet.h>

FrameFromFile::FrameFromFile(istream &fin, control_panel_t &panel)
  : FrameCycle(panel), fin(fin), tj_dec(tjInitDecompress())  {
}

FrameFromFile::~FrameFromFile() {
  tjDestroy(this->tj_dec);
}

bool FrameFromFile::process_frame() {

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
  vector< char > buffer(length);
  this->fin.read(&buffer[0], length);
  if (this->fin.gcount() != length) {
    return false;
  }

}
