
#include "worker.h"
#include "logging.h"

static int safe_ideal_thread_count() {
    int res = QThread::idealThreadCount();
    if (res < 0) {
        res = 1;
    }
    return res;
}

Worker::Worker(const FrameSettings &settings) :
  jpeg_reader("test.gjpeg", true, true),
  context(safe_ideal_thread_count(), &jpeg_reader, settings),
  running(true)
{
}

void Worker::set_settings(const FrameSettings &settings) {
    this->context.set_settings(settings);
}

void Worker::stop() {
    this->jpeg_reader.stop();
}

void Worker::run() {
    BOOST_LOG_NAMED_SCOPE("worker run");
    this->jpeg_reader.start();
    while (this->running) {
        FrameAnalysis *frame = this->context.get();
        if (frame == NULL) {
            BOOST_LOG_TRIVIAL(debug) << "Worker ready to exit";
            break;
        }
        QSharedPointer< FrameAnalysis > frame_ptr(frame);
        //BOOST_LOG_TRIVIAL(debug) << "Emitting frame";
        emit frame_produced(frame_ptr);
    }
}
