#ifndef _JPEGREADER_HPP
#define _JPEGREADER_HPP

#include "framereader.hpp"

#include <turbojpeg.h>

class FrameFromFile: public FrameCycle {
private:
  istream &fin;
  tjhandle tj_dec;
  time_point< system_clock > first_frame_time;
  bool first_frame_seen = false;

protected:
  bool process_frame();

public:
  FrameFromFile(istream &fin, control_panel_t &panel);
  ~FrameFromFile();
};

#endif
