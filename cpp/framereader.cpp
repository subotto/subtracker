
#include "framereader.hpp"
#include "v4l2cap.hpp"

FrameCycle::FrameCycle(control_panel_t &panel, bool droppy)
  : panel(panel), last_stats(system_clock::now()), running(true), droppy(droppy) {
}

void FrameCycle::start() {
  this->t = thread(&FrameCycle::cycle, this);
}

FrameReader::FrameReader(int device, control_panel_t& panel, int width, int height, int fps)
	: FrameCycle(panel) {
  cap = v4l2cap(device, width, height, fps);
}

FrameReader::FrameReader(const char* file, control_panel_t& panel, bool simulate_live)
	: FrameCycle(panel) {
  cap = VideoCapture(file);
  if(!cap.isOpened()) {
    fprintf(stderr, "Cannot open video file!\n");
    exit(1);
  }
  this->fromFile = true;
  this->rate_limited = simulate_live;
  this->can_drop_frames = simulate_live;
}

bool FrameCycle::init_thread() {

  return true;

}

void FrameCycle::terminate() {

  this->push({ time_point< system_clock >(), time_point< system_clock >(), Mat(), false });

}

void FrameCycle::cycle() {

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

bool FrameReader::init_thread() {

  if (!fromFile) {
    // Delay di 1s per dare il tempo alla webcam di inizializzarsi
    this_thread::sleep_for(seconds(1));
  }
  logger(panel, "gio", DEBUG) << "Start receiving frames" << endl;
  return true;

}

bool FrameReader::process_frame() {

  Mat frame;
  if(!cap.read(frame)) {
    return false;
  }
  auto now = system_clock::now();

  time_point<system_clock> time, playback_time;
  if(fromFile) {
    double posMsec = cap.get(CV_CAP_PROP_POS_MSEC);
    playback_time = this->video_start_playback_time + duration_cast< time_point< system_clock >::duration >(duration<double, milli>(posMsec));
    // Since we are taking the input from a file which is not
    // timestamped, we don't know the original time of the frame; we
    // then use playback_time
    time = playback_time;
  } else {
    time = now;
    playback_time = now;
  }

  this->push({ time, playback_time, frame, true });
  return true;

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

  logger(panel, "capture", INFO) << "queue size: " << queue.size() << endl;
  logger(panel, "capture", INFO) <<
    "received " << frame_times.size() << " frames " <<
    "in " << seconds(frame_count_interval).count() << " seconds (" <<
    frame_times.size() / seconds(frame_count_interval).count() <<
    " frames per second)" << endl;
  logger(panel, "capture", INFO) <<
    "processed " << enqueued_frames - queue.size() << " frames " <<
    "in " << duration_cast<duration<float>>(now - video_start_playback_time).count() << " seconds (" <<
    (enqueued_frames - queue.size()) / duration_cast<duration<float>>(now - video_start_playback_time).count() <<
    " frames per second)." << endl;
  if(frame_dropped.size())
    logger(panel, "capture", WARNING) << "dropped " <<
      frame_dropped.size() << " in " <<
      seconds(frame_count_interval).count() << " seconds (" <<
      frame_dropped.size() / seconds(frame_count_interval).count() <<
      " frames per second)." << endl;

}

void FrameCycle::push(FrameInfo info) {

  this->stats(info);

  unique_lock<mutex> lock(queue_mutex);

  if (this->droppy) {
    queue.clear();
  }

  if (!can_drop_frames) {
    while (queue.size() >= buffer_size) {
      queue_not_full.wait(lock);
      logger(panel, "capture", INFO) << "queue is full, waiting" << endl;
    }
  }

  if(rate_limited) {
    logger(panel, "capture", DEBUG) << "rate limiting for " << duration_cast< duration< double > >(info.playback_time - system_clock::now()).count() << " seconds" << endl;
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
    logger(panel, "capture", DEBUG) << "frame dropped" << endl;
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

FrameCycle::~FrameCycle() {
  running = false;
  if (t.joinable()) {
    t.join();
  }
}

void FrameCycle::set_droppy(bool droppy) {

  this->droppy = droppy;

}
