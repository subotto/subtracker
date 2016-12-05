
#include "worker.h"
#include "logging.h"

static int safe_ideal_thread_count() {
    // debug and test
    //return 1;
    int res = QThread::idealThreadCount();
    if (res < 0) {
        res = 1;
    }
    return res;
}

Worker::Worker(const FrameSettings &settings) :
  jpeg_reader("test.gjpeg", true, true),
  context(safe_ideal_thread_count(), &jpeg_reader, settings),
  last_frame(), running(true)
{
}

void Worker::set_settings(const FrameSettings &settings) {
    this->context.set_settings(settings);
}

void Worker::stop() {
    this->jpeg_reader.stop();
}

QSharedPointer< FrameAnalysis > Worker::get_last_frame() {
    return this->last_frame;
}

void Worker::run() {
    BOOST_LOG_NAMED_SCOPE("worker run");
    this->jpeg_reader.start();
    while (this->running) {
        //BOOST_LOG_TRIVIAL(debug) << "Waiting frame";
        FrameAnalysis *frame = this->context.get();
        //BOOST_LOG_TRIVIAL(debug) << "Got frame";
        if (this->context.is_finished()) {
            BOOST_LOG_TRIVIAL(debug) << "Worker ready to exit";
            break;
        }
        this->last_frame = QSharedPointer<FrameAnalysis>(frame);
        emit frame_produced();
    }
}
