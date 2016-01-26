#ifndef _JPEGREADER_HPP
#define _JPEGREADER_HPP

#include "framereader.hpp"

#include <turbojpeg.h>

#include <fstream>

class JPEGReader: public FrameCycle {
private:
  Ptr< istream > fin;
  tjhandle tj_dec;
  time_point< system_clock > first_frame_time;
  bool first_frame_seen = false;
  bool from_file;

  int width;
  int height;

protected:
  bool process_frame();
  void open_file(string file_name);

public:
  JPEGReader(string file_name, control_panel_t &panel, bool from_file, bool simulate_live, int width=-1, int height=-1);
  ~JPEGReader();
  static void mangle_file_name(string &file_name, bool &from_file, bool &simulate_live);
};

#endif
