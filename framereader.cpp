
#include "framereader.hpp"

static const seconds frame_count_interval(5);
static const int stats_interval = 1;
static const int buffer_size = 500;

FrameReader::FrameReader(int device, control_panel_t& panel)
	: panel(panel) {
  running = true;
  count = 0;
  last_stats = high_resolution_clock::now();
  cap = v4l2cap(device, 320, 240, 125);
  t = thread(&FrameReader::read, this);
}

FrameReader::FrameReader(const char* file, control_panel_t& panel, bool simulate_live)
	: panel(panel) {
  running = true;
  count = 0;
  last_stats = high_resolution_clock::now();
  cap = VideoCapture(file);
  if(!cap.isOpened()) {
    fprintf(stderr, "Cannot open video file!\n");
    exit(1);
  }
  t = thread(&FrameReader::read, this);

  this->fromFile = true;
  this->rate_limited = simulate_live;
  this->can_drop_frames = simulate_live;
}

void FrameReader::read() {
  if(!fromFile) {
    // Delay di 1s per dare il tempo alla webcam di inizializzarsi
    this_thread::sleep_for(seconds(1));
  }
  logger(panel, "gio", DEBUG) << "Start receiving frames" << endl;
  video_start_time = system_clock::now();
  long unsigned int enqueued_frames = 0;
  while(running) {
    Mat frame;
    if(!cap.read(frame)) {
      if(fromFile) {
        logger(panel, "capture", INFO) << "Video ended." << endl;
        exit(0);
      } else {
        logger(panel, "capture", ERROR) << "Error reading frame!" << endl;
        continue;
      }
    }
    count++;
    auto now = system_clock::now();

    time_point<video_clock> timestamp;
    if(fromFile) {
      double posMsec = cap.get(CV_CAP_PROP_POS_MSEC);
      timestamp = time_point<video_clock>(duration_cast<nanoseconds>(duration<double, milli>(posMsec)));
    } else {
      timestamp = time_point<video_clock>(now - video_start_time);
    }

    logger(panel, "gio", DEBUG) << "Read new frame with timestamp " << duration_cast<duration<double, milli> >(timestamp.time_since_epoch()).count() << endl;

    frame_times.push_back(timestamp);
    while(timestamp - frame_times.front() > frame_count_interval) {
      frame_times.pop_front();
    }

    while(frame_dropped.size()  && timestamp - frame_dropped.front() > frame_count_interval)
      frame_dropped.pop_front();

    if(now - last_stats > seconds(stats_interval)) {
      logger(panel, "capture", INFO) << "queue size: " << queue.size() << endl;
      logger(panel, "capture", INFO) <<
        "received " << frame_times.size() << " frames " <<
        "in " << seconds(frame_count_interval).count() << " seconds (" <<
        frame_times.size() / seconds(frame_count_interval).count() <<
        " frames per second)" << endl;
      logger(panel, "capture", INFO) <<
        "processed " << enqueued_frames - queue.size() << " frames " <<
        "in " << duration_cast<duration<float>>(now - video_start_time).count() << " seconds (" <<
        (enqueued_frames - queue.size()) / duration_cast<duration<float>>(now - video_start_time).count() <<
        " frames per second)." << endl;
      if(frame_dropped.size())
        logger(panel, "capture", WARNING) << "dropped " <<
          frame_dropped.size() << " in " <<
          seconds(frame_count_interval).count() << " seconds (" <<
          frame_dropped.size() / seconds(frame_count_interval).count() <<
          " frames per second)." << endl;
      last_stats = now;
    }

    unique_lock<mutex> lock(queue_mutex);
    if (!can_drop_frames) {
      while (queue.size() >= buffer_size) {
        queue_not_full.wait(lock);
        logger(panel, "capture", INFO) << "queue is full, waiting" << endl;
      }
    }

    auto playback_time = video_start_time + timestamp.time_since_epoch();

    if(rate_limited) {
      this_thread::sleep_until(playback_time);
    }

    // Prende tutti i frames finchè queue.size() < buffer_size, un
    // frame su 2 se queue.size() < 2*buffer_size, uno su 3 se
    // queue.size() < 3*buffer_size, etc
    if (count % (queue.size() / buffer_size + 1) == 0) {
      queue.push_back( { timestamp, playback_time, frame });
      queue_not_empty.notify_all();
      enqueued_frames++;
    } else {
      assert(can_drop_frames);
      frame_dropped.push_back(timestamp);
      logger(panel, "capture", DEBUG) << "frame dropped" << endl;
    }
  }
}

frame_info FrameReader::get() {
  unique_lock<mutex> lock(queue_mutex);
  while(queue.empty()) {
    queue_not_empty.wait(lock);
  }
  auto res = queue.front();
  queue.pop_front();
  queue_not_full.notify_all();

  logger(panel, "gio", DEBUG) << "Return frame with timestamp " << duration_cast<duration<double, milli> >(res.timestamp.time_since_epoch()).count() << endl;

  return res;
}

FrameReader::~FrameReader() {
  running = false;
  t.join();
}
