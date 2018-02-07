
#include <chrono>
#include <sstream>
#include <utility>

#include <opencv2/core/core.hpp>

#include "context.h"
#include "logging.h"
#include "atomiccounter.h"

using namespace std;
using namespace chrono;
using namespace cv;

Context::Context(size_t slave_num, FrameProducer *producer, const FrameSettings &settings) :
    active_threads_num(0), exhausted(false),
    frame_num(0), settings(settings),
    producer(producer), output(NULL)
{
    for (size_t i = 0; i < slave_num; i++) {
        ostringstream oss;
        oss << "worker " << i;
        this->create_thread(oss.str(), &Context::working_thread, this);
    }
}

template< class Function, class... Args >
void Context::create_thread(string name, Function&& f, Args&&... args) {
    this->slaves.emplace_back(f, args...);

    // Comment out the following if pthreads is not the underlying thread implementation
    // Warning: thread names cannot be longer than 16 characters
    // Failure is ignored
    pthread_t handle = this->slaves.back().native_handle();
    pthread_setname_np(handle, name.c_str());
}

Context::~Context() {
    for (auto &thread : this->slaves) {
        thread.join();
    }
    delete this->output;
    for (auto &frame : this->waiting_frames) {
        delete frame;
    }
}

void Context::set_settings(const FrameSettings &settings) {
    unique_lock< mutex > lock(this->settings_mutex);
    this->settings = settings;
}

std::pair<unique_lock< mutex >, FrameCommands*> Context::edit_commands()
{
    // We need to first manually lock the mutex, because the parameter evaluation order of the return clause is not specified
    this->settings_mutex.lock();
    return make_pair(unique_lock< mutex >(this->settings_mutex, adopt_lock), &this->commands);
}

bool Context::is_finished()
{
    return this->exhausted && this->active_threads_num == 0;
}

void Context::working_thread()
{
    BOOST_LOG_NAMED_SCOPE("working thread");
    /* AtomicCounter automatically acquires the mutex and notifies the output_full condition,
     * otherwise a race happens for get()-ing threads that have already acquired the lock
     * but not yet waited for output_full.
     */
    AtomicCounter counter(this->active_threads_num, this->output_mutex, this->output_full);
    ThreadContext thread_ctx;

    while (true) {
        int frame_num;
        FrameInfo info;
        FrameSettings settings;
        FrameCommands commands;
        system_clock::time_point acquisition_time;
        steady_clock::time_point acquisition_steady_time;
        {
            /* This lock is unfortunately rather long, because it contains the JPEG decoding procedure.
             * This is due to the fact that we need to know if the decoding succeed before
             * incrementing this->frame_num, in order to not increment it if the frame is invalid.
             * However, frame_num incrementing must stay in the same critical region as producer->get(),
             * otherwise the numbering monotonicity can fail.
             */
            unique_lock< mutex > lock(this->get_frame_mutex);
            if (this->frame_ctx.have_fix) {
                info = this->producer->get();
            } else {
                // If the anaysis has already lost the fix, processing will have to begin again anyway, so we can discard the queue
                // FIXME - This does not wait for a frame if the queue is empty
                info = this->producer->get_last_at_least_one();
            }
            acquisition_time = system_clock::now();
            acquisition_steady_time = steady_clock::now();
            if (!info.valid) {
                BOOST_LOG_TRIVIAL(debug) << "Found terminator";
                unique_lock< mutex > lock(this->output_mutex);
                this->exhausted = true;
                this->output_full.notify_all();
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
            commands = this->commands;
            this->commands = FrameCommands();
        }

        FrameAnalysis *frame = new FrameAnalysis(info.data, frame_num, info.time, acquisition_time, acquisition_steady_time, settings, commands, this->frame_ctx, thread_ctx);
        frame->do_things();

        {
            FrameWaiter waiter(this->spots_waiter, frame_num);
            this->spots_tracker.push_back(frame->get_spots(), frame->get_time().to_double());
            this->waiting_frames.push_back(frame);
            while (this->waiting_frames.size() > this->settings.spots_tracking_len) {
                bool valid;
                Point2f ball;
                tie(valid, ball) = this->spots_tracker.front();
                int front_num = this->spots_tracker.get_front_num();
                this->spots_tracker.pop_front();
                FrameAnalysis *out_frame = this->waiting_frames.front();
                this->waiting_frames.pop_front();
                assert(front_num == out_frame->get_frame_num());
                out_frame->set_ball(valid, ball);
                out_frame->do_rendering();

                {
                    FrameWaiter waiter(this->output_waiter, front_num);
                    unique_lock< mutex > lock(this->output_mutex);
                    while (this->output != NULL) {
                        this->output_empty.wait(lock);
                    }
                    this->output = out_frame;
                    this->output_full.notify_one();
                }
            }
        }
    }
}

FrameAnalysis *Context::get() {
    FrameAnalysis *frame;
    unique_lock< mutex > lock(this->output_mutex);
    while (this->output == NULL) {
        if (this->is_finished()) {
            return NULL;
        }
        this->output_full.wait(lock);
    }
    frame = this->output;
    this->output = NULL;
    this->output_empty.notify_all();
    return frame;
}

void Context::wait() {
    unique_lock< mutex > lock(this->output_mutex);
    while (this->output == NULL) {
        if (this->is_finished()) {
            return;
        }
        this->output_full.wait(lock);
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
