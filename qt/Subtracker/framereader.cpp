
#include "framereader.h"
#include "logging.h"

#include <arpa/inet.h>
#include <boost/asio.hpp>

using namespace std;
using namespace chrono;
using namespace cv;

FrameCycle::FrameCycle(bool droppy)
  : last_stats(system_clock::now()), running(true), finished(false), droppy(droppy) {
}

void FrameCycle::start() {
  this->t = thread(&FrameCycle::cycle, this);
}

bool FrameCycle::init_thread() {

  return true;

}

void FrameCycle::terminate() {

  this->push({ time_point< system_clock >(), time_point< system_clock >(), Mat(), false });
  this->finished = true;

}

bool FrameCycle::is_finished() {
  return this->finished;
}

void FrameCycle::cycle() {

  BOOST_LOG_NAMED_SCOPE("capture cycle");

  video_start_playback_time = system_clock::now();
  if (!this->init_thread()) {
    this->terminate();
    return;
  }
  while (running) {
    if (!this->process_frame()) {
      this->terminate();
      this->process_stats();
      return;
    }
  }

}

void FrameCycle::stats(const FrameInfo &info) {

  auto now = system_clock::now();

  frame_times.push_back(info.playback_time);

  if (now - last_stats > stats_interval) {
    this->process_stats();
    last_stats = now;
  }

}

void FrameCycle::process_stats() {

  auto now = system_clock::now();

  while(frame_times.back() - frame_times.front() > frame_count_interval) {
    frame_times.pop_front();
  }
  while(frame_dropped.size() && frame_times.back() - frame_dropped.front() > frame_count_interval) {
    frame_dropped.pop_front();
  }

  BOOST_LOG_TRIVIAL(info) << "queue size: " << queue.size();
  BOOST_LOG_TRIVIAL(info) <<
    "received " << frame_times.size() << " frames " <<
    "in " << seconds(frame_count_interval).count() << " seconds (" <<
    frame_times.size() / seconds(frame_count_interval).count() <<
    " frames per second)";
  BOOST_LOG_TRIVIAL(info) <<
    "processed " << enqueued_frames - queue.size() << " frames " <<
    "in " << duration_cast<duration<float>>(now - video_start_playback_time).count() << " seconds (" <<
    (enqueued_frames - queue.size()) / duration_cast<duration<float>>(now - video_start_playback_time).count() <<
    " frames per second).";
  if(frame_dropped.size())
    BOOST_LOG_TRIVIAL(warning) << "dropped " <<
      frame_dropped.size() << " in " <<
      seconds(frame_count_interval).count() << " seconds (" <<
      frame_dropped.size() / seconds(frame_count_interval).count() <<
      " frames per second).";

}

void FrameCycle::push(FrameInfo info) {

  BOOST_LOG_NAMED_SCOPE("frame push");

  this->stats(info);

  unique_lock<mutex> lock(queue_mutex);

  if (this->droppy) {
    queue.clear();
  }
  if (!this->running) {
    return;
  }

  if (!can_drop_frames) {
    while (queue.size() >= buffer_size) {
      queue_not_full.wait(lock);
      if (!this->running) {
        return;
      }
      BOOST_LOG_TRIVIAL(info) << "queue is full, waiting";
    }
  }

  if(rate_limited) {
    //BOOST_LOG_TRIVIAL(debug) << "rate limiting for " << duration_cast< duration< double > >(info.playback_time - system_clock::now()).count() << " seconds";
    this_thread::sleep_until(info.playback_time);
  }

  // Prende tutti i frames finchè queue.size() < buffer_size, un frame
  // su 2 se queue.size() < 2*buffer_size, uno su 3 se queue.size() <
  // 3*buffer_size, etc; in ogni caso accettiamo i frame non validi
  // (perché segnalano la fine dello stream)
  if (count % (queue.size() / buffer_size + 1) == 0 || !info.valid) {
    queue.push_back(info);
    queue_not_empty.notify_all();
    enqueued_frames++;
  } else {
    assert(can_drop_frames);
    frame_dropped.push_back(info.playback_time);
    //BOOST_LOG_TRIVIAL(debug) << "frame dropped";
  }
  count++;

}

FrameInfo FrameCycle::get() {
  unique_lock<mutex> lock(queue_mutex);
  while(queue.empty()) {
    queue_not_empty.wait(lock);
  }
  auto res = queue.front();
  queue.pop_front();
  queue_not_full.notify_all();
  return res;
}

FrameInfo FrameCycle::maybe_get() {
  unique_lock<mutex> lock(this->queue_mutex);
  if (queue.empty()) {
      return { time_point< system_clock >(), time_point< system_clock >(), Mat(), false };
  }
  auto res = queue.front();
  queue.pop_front();
  queue_not_full.notify_all();
  return res;
}

FrameInfo FrameCycle::get_last() {
  unique_lock<mutex> lock(this->queue_mutex);
  FrameInfo res = { time_point< system_clock >(), time_point< system_clock >(), Mat(), false };
  while (!queue.empty()) {
    res = queue.front();
    queue.pop_front();
  }
  queue_not_full.notify_all();
  return res;
}

void FrameCycle::stop() {
  this->running = false;
  this->queue_not_full.notify_all();
}

void FrameCycle::join_worker() {
  this->stop();
  if (t.joinable()) {
    t.join();
  }
}

FrameCycle::~FrameCycle() {

  this->join_worker();
}

void FrameCycle::set_droppy(bool droppy) {

  this->droppy = droppy;

}

JPEGReader::JPEGReader(string file_name, bool from_file, bool simulate_live, int width, int height)
  : tj_dec(tjInitDecompress()), from_file(from_file), width(width), height(height) {

  BOOST_LOG_NAMED_SCOPE("jpeg open");

  if (from_file) {
    if (simulate_live) {
      this->rate_limited = true;
    } else {
      this->can_drop_frames = false;
    }
  }

  this->open_file(file_name);

  BOOST_LOG_TRIVIAL(debug) << "can_drop_frames: " << this->can_drop_frames << "; rate_limited: " << this->rate_limited;

}

void JPEGReader::open_file(string file_name) {

  string socket_marker = "socket://";
  if (file_name.substr(0, socket_marker.size()) == socket_marker) {
    file_name = file_name.substr(socket_marker.size());
    int colon_idx = file_name.find(":");
    string host = file_name.substr(0, colon_idx);
    string port = file_name.substr(colon_idx + 1);
    BOOST_LOG_TRIVIAL(info) << "Connecting to " << host << " : " << port;
    auto *fin = new boost::asio::ip::tcp::iostream(host, port);
    if (!(*fin)) {
      BOOST_LOG_TRIVIAL(error) << "Unable to connect: " << fin->error().message();
    }
    this->fin = fin;
  } else {
    BOOST_LOG_TRIVIAL(info) << "Opening file " << file_name;
    this->fin = new ifstream(file_name);
  }

}

void JPEGReader::mangle_file_name(string &file_name, bool &from_file, bool &simulate_live) {

  from_file = true;
  simulate_live = false;
  if (file_name.back() == '+') {
    file_name = file_name.substr(0, file_name.size()-1);
    simulate_live = true;
  } else if (file_name.back() == '-') {
    file_name = file_name.substr(0, file_name.size()-1);
    from_file = false;
  }

}

JPEGReader::~JPEGReader() {

  this->join_worker();
  tjDestroy(this->tj_dec);

}

bool JPEGReader::process_frame() {

  BOOST_LOG_NAMED_SCOPE("jpeg process");

  // Read from stream
  double timestamp;
  uint32_t length;
  this->fin->read((char*) &timestamp, sizeof(double));
  if (this->fin->gcount() != sizeof(double)) {
    BOOST_LOG_TRIVIAL(error) << "Cannot read timestamp";
    return false;
  }
  this->fin->read((char*) &length, sizeof(uint32_t));
  if (this->fin->gcount() != sizeof(uint32_t)) {
    BOOST_LOG_TRIVIAL(error) << "Cannot read length";
    return false;
  }
  length = ntohl(length);
  // See http://stackoverflow.com/a/30605295/807307
  vector< char > buffer(length);
  this->fin->read(&buffer[0], length);
  if (this->fin->gcount() != length) {
    BOOST_LOG_TRIVIAL(error) << "Cannot read data";
    return false;
  }

  // Actually decode image
  FrameInfo info;
  int width, height, subsamp, res;
  res = tjDecompressHeader2(this->tj_dec, (unsigned char*) &buffer[0], length, &width, &height, &subsamp);
  if (res) {
    BOOST_LOG_TRIVIAL(warning) << "Cannot decompress JPEG header, skipping frame";
    return true;
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
    BOOST_LOG_TRIVIAL(warning) << "Cannot decompress JPEG image, skipping frame";
    return true;
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
  return true;

}
