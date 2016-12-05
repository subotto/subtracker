#include "context.h"
#include "logging.h"
#include "atomiccounter.h"

#include <chrono>

using namespace std;
using namespace chrono;

Context::Context(size_t slave_num, FrameProducer *producer, const FrameSettings &settings) :
    running(true),
    frame_num(0), settings(settings),
    producer(producer), output(NULL)
{
    /*this->create_thread("", &Context::phase1_thread, this);
    this->create_thread("", &Context::phase3_thread, this);
    for (int i = 0; i < slave_num; i++) {
        this->create_thread("", &Context::phase2_thread, this);
    }
    for (int i = 0; i < slave_num; i++) {
        this->create_thread("", &Context::phase0_thread, this);
    }*/
    for (size_t i = 0; i < slave_num; i++) {
        this->create_thread("working thread", &Context::working_thread, this);
    }
}

template< class Function, class... Args >
void Context::create_thread(string name, Function&& f, Args&&... args) {
    this->slaves.emplace_back(f, args...);

    // Comment out the following if pthreads is not the underlying thread implementation
    pthread_t handle = this->slaves.back().native_handle();
    pthread_setname_np(handle, name.c_str());
}

Context::~Context() {
    this->running = false;
    for (auto &thread : this->slaves) {
        thread.join();
    }
    delete this->output;
}

void Context::set_settings(const FrameSettings &settings) {
    unique_lock< mutex > lock(this->settings_mutex);
    this->settings = settings;
}

bool Context::is_finished()
{
    return !this->running;
}

void Context::working_thread()
{
    BOOST_LOG_NAMED_SCOPE("working thread");
    ThreadContext thread_ctx;

    while (true) {
        int frame_num;
        FrameInfo info;
        FrameSettings settings;
        system_clock::time_point acquisition_time;
        steady_clock::time_point acquisition_steady_time;
        {
            /* This lock is unfortunately rather long, because it contains the JPEG decoding procedure.
             * This is due to the fact that we need to know if the decoding succeed before
             * incrementing this->frame_num, in order to not increment it if the frame is invalid.
             * However, frame_num incrementing must stay in the same critical region as producer->get(),
             * otherwise the numbering monotonicity can file.
             */
            unique_lock< mutex > lock(this->get_frame_mutex);
            info = this->producer->get();
            acquisition_time = system_clock::now();
            acquisition_steady_time = steady_clock::now();
            if (!info.valid) {
                BOOST_LOG_TRIVIAL(debug) << "Found terminator";
                return;
            }
            bool res = info.decode_buffer(thread_ctx.tj_dec);
            //bool res = true;
            if (!res) {
                /* The frame had some error, we just skip it (and we do not increment this->frame_num,
                 * as later stages assume that frame_num are contiguous).
                 */
                BOOST_LOG_TRIVIAL(info) << "Skipping broken frame";
                continue;
            }
            frame_num = this->frame_num++;

            /* The settings lock is acquired later, because it is used in the UI thread. Still it must
             * stay in the get_frame lock, so that also settings are acquired monotonically.
             */
            unique_lock< mutex > lock2(this->settings_mutex);
            settings = this->settings;
        }

        FrameAnalysis *frame = new FrameAnalysis(info.data, frame_num, info.time, settings);
        frame->acquisition_time = acquisition_time;
        frame->do_things(this->frame_ctx, thread_ctx);

        {
            FrameWaiter waiter(this->output_waiter, frame->frame_num);
            unique_lock< mutex > lock(this->output_mutex);
            while (this->output != NULL) {
                if (!this->running) {
                    return;
                }
                this->output_empty.wait_for(lock, seconds(1));
            }
            this->output = frame;
            this->output_full.notify_one();
        }
    }
}

FrameAnalysis *Context::get() {
    FrameAnalysis *frame;
    unique_lock< mutex > lock(this->output_mutex);
    while (this->output == NULL) {
        if (!this->running) {
            return NULL;
        }
        this->output_full.wait_for(lock, seconds(1));
    }
    frame = this->output;
    this->output = NULL;
    this->output_empty.notify_all();
    return frame;
}

void Context::wait() {
    unique_lock< mutex > lock(this->output_mutex);
    while (this->output == NULL) {
        if (!this->running) {
            return;
        }
        this->output_full.wait_for(lock, seconds(1));
    }
}

FrameAnalysis *Context::maybe_get() {
    FrameAnalysis *frame;
    unique_lock< mutex > lock(this->output_mutex);
    if (this->output == NULL) {
        return NULL;
    }
    frame = this->output;
    this->output = NULL;
    this->output_empty.notify_all();
    return frame;
}
