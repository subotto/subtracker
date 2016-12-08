
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

    this->finished = true;
    unique_lock<mutex> lock(this->queue_mutex);
    this->queue_not_empty.notify_all();

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
  while (true) {
    if (!this->process_frame() || !running) {
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

  if(rate_limited && info.valid) {
    BOOST_LOG_TRIVIAL(debug) << "rate limiting for " << duration_cast< duration< double > >(info.playback_time - system_clock::now()).count() << " seconds";
    this_thread::sleep_until(info.playback_time);
  }

  BOOST_LOG_TRIVIAL(debug) << "Request lock";
  unique_lock<mutex> lock(queue_mutex);
  BOOST_LOG_TRIVIAL(debug) << "Received lock";

  if (this->droppy) {
        queue.clear();
  }
  if (!this->running && info.valid) {
        return;
  }

  if (!can_drop_frames && info.valid) {
    while (queue.size() >= buffer_size) {
      BOOST_LOG_TRIVIAL(debug) << "Waiting";
      queue_not_full.wait(lock);
      BOOST_LOG_TRIVIAL(debug) << "Wait finished";
      if (!this->running) {
        return;
      }
      BOOST_LOG_TRIVIAL(info) << "queue is full, waiting";
    }
  }

  BOOST_LOG_TRIVIAL(debug) << "Can do";

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
  BOOST_LOG_TRIVIAL(debug) << "Finished";

}

FrameInfo FrameCycle::get() {
  BOOST_LOG_NAMED_SCOPE("frame get");
  BOOST_LOG_TRIVIAL(debug) << "Request lock";
  unique_lock<mutex> lock(queue_mutex);
  BOOST_LOG_TRIVIAL(debug) << "Received lock";
  while (queue.empty()) {
      if (this->finished) {
          return { time_point< system_clock >(), time_point< system_clock >(), NULL, Mat(), false };
      }
      BOOST_LOG_TRIVIAL(debug) << "Waiting";
      queue_not_empty.wait(lock);
      BOOST_LOG_TRIVIAL(debug) << "Wait finished";
  }
  BOOST_LOG_TRIVIAL(debug) << "Can do";
  auto res = queue.front();
  queue.pop_front();
  queue_not_full.notify_all();
  BOOST_LOG_TRIVIAL(debug) << "Finished";
  return res;
}

FrameInfo FrameCycle::maybe_get() {
  unique_lock<mutex> lock(this->queue_mutex);
  if (queue.empty()) {
      return { time_point< system_clock >(), time_point< system_clock >(), NULL, Mat(), false };
  }
  auto res = queue.front();
  queue.pop_front();
  queue_not_full.notify_all();
  return res;
}

FrameInfo FrameCycle::get_last() {
  unique_lock<mutex> lock(this->queue_mutex);
  FrameInfo res = { time_point< system_clock >(), time_point< system_clock >(), NULL, Mat(), false };
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

int FrameCycle::get_queue_length()
{
    return this->queue.size();
}

void FrameCycle::kill_queue()
{
    // First swap the queue with a temporary one, so the lock is released as quickly as possible;
    // then queue items are deallocated out of the lock, when this function returns.
    // No conditions are notified, because this function is meant to be used when the FrameReader is not running.
    deque< FrameInfo > queue_swap;
    {
        unique_lock<mutex> lock(this->queue_mutex);
        this->queue.swap(queue_swap);
    }
}

JPEGReader::JPEGReader(string file_name, bool from_file, bool simulate_live, int width, int height)
  : FrameCycle(false), from_file(from_file), width(width), height(height) {

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

}

bool JPEGReader::process_frame() {

  FrameInfo info;

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
  info.buffer = shared_ptr< vector< char > >(new vector< char >(length));
  this->fin->read(&(*info.buffer)[0], length);
  if (this->fin->gcount() != length) {
    BOOST_LOG_TRIVIAL(error) << "Cannot read data";
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
  return true;

}

bool FrameInfo::decode_buffer(tjhandle tj_dec) {

    BOOST_LOG_NAMED_SCOPE("jpeg decode buffer");

    // Actually decode image
    int width, height, subsamp, res;
    int length = this->buffer->size();
    res = tjDecompressHeader2(tj_dec, (unsigned char*) &(*this->buffer)[0], length, &width, &height, &subsamp);
    if (res) {
      BOOST_LOG_TRIVIAL(warning) << "Cannot decompress JPEG header (" << tjGetErrorStr() << ")";
      return false;
    }
    this->data = Mat(height, width, CV_8UC3);
    assert(this->data.elemSize() == 3);
    res = tjDecompress2(tj_dec, (unsigned char*) &(*this->buffer)[0], length, this->data.data, width, this->data.step[0], height, TJPF_BGR, TJFLAG_ACCURATEDCT);
    if (res) {
      BOOST_LOG_TRIVIAL(warning) << "Cannot decompress JPEG image (" << tjGetErrorStr() << ")";
      return false;
    }

    return true;

}
