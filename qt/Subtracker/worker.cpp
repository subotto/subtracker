
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

QSharedPointer< FrameAnalysis > Worker::maybe_get() {
    return QSharedPointer< FrameAnalysis >(this->context.maybe_get());
}

void Worker::run() {
    BOOST_LOG_NAMED_SCOPE("worker run");
    this->jpeg_reader.start();
    while (this->running) {
        this->context.wait();
        if (this->context.is_finished()) {
            BOOST_LOG_TRIVIAL(debug) << "Worker ready to exit";
            break;
        }
        emit frame_produced();
    }
}
