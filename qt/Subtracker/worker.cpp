
#include "worker.h"
#include "logging.h"

static int safe_ideal_thread_count() {
    int res = QThread::idealThreadCount();
    if (res < 0) {
        res = 1;
    }
    return res;
}

Worker::Worker() :
    jpeg_reader("test.gjpeg", true, true),
    context(safe_ideal_thread_count(), &jpeg_reader)
{
}

void Worker::run() {
    BOOST_LOG_NAMED_SCOPE("when frame produced");
    this->jpeg_reader.start();
    while (true) {
        FrameAnalysis *frame = this->context.get();
        QSharedPointer< FrameAnalysis > frame_ptr(frame);
        BOOST_LOG_TRIVIAL(debug) << "Emitting frame";
        emit frame_produced(frame_ptr);
    }
}
