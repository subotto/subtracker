
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

Worker::Worker(const FrameSettings &settings, std::ostream &out_stream) :
  jpeg_reader("socket://127.0.0.1:2204", true, true),
  context(safe_ideal_thread_count(), &jpeg_reader, settings),
  last_frame(), running(true),
  out_stream(out_stream)
{
}

void Worker::set_settings(const FrameSettings &settings) {
    this->context.set_settings(settings);
}

std::pair<std::unique_lock<std::mutex>, FrameCommands *> Worker::edit_commands()
{
    return this->context.edit_commands();
}

void Worker::stop() {
    this->jpeg_reader.stop();
    this->jpeg_reader.kill_queue();
}

QSharedPointer< FrameAnalysis > Worker::get_last_frame() {
    return this->last_frame;
}

int Worker::get_queue_length()
{
    return this->jpeg_reader.get_queue_length();
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
        if (frame->does_have_fix()) {
            this->out_stream << frame->gen_csv_line() << "\n";
        }
        this->last_frame = QSharedPointer<FrameAnalysis>(frame);
        emit frame_produced();
    }
}
