#ifndef _JPEGREADER_HPP
#define _JPEGREADER_HPP

#include "framereader.hpp"

#include <turbojpeg.h>

class FrameFromFile: public FrameCycle {
private:
  istream &fin;
  tjhandle tj_dec;

protected:
  bool process_frame();

public:
  FrameFromFile(istream &fin, control_panel_t &panel);
  ~FrameFromFile();
};

#endif
