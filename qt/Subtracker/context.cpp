#include "context.h"
#include "logging.h"
#include "atomiccounter.h"

#include <chrono>

using namespace std;
using namespace chrono;

Context::Context(int slave_num, FrameProducer *producer, const FrameSettings &settings) :
    running(true),
    phase0_out(NULL), phase1_out(NULL), phase2_out(NULL), phase3_out(NULL),
    phase0_count(0), phase1_count(0), phase2_count(0), phase3_count(0),
    exausted(false), frame_num(0), settings(settings),
    producer(producer), phase3_frame_num(0)
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
    delete this->phase0_out;
    delete this->phase1_out;
    delete this->phase2_out;
    delete this->phase3_out;
}

void Context::set_settings(const FrameSettings &settings) {
    unique_lock< mutex > lock(this->settings_mutex);
    this->settings = settings;
}

bool Context::is_finished()
{
    return (!this->running || (this->phase3_count == 0 && this->exausted));
}

void Context::phase0_thread() {

    BOOST_LOG_NAMED_SCOPE("phase0 thread");

    AtomicCounter counter(this->phase0_count);
    tjhandle tj_dec = tjInitDecompress();

    while (true) {
        int frame_num;
        FrameInfo info;
        FrameSettings settings;
        system_clock::time_point acquisition_time, begin_time;
        steady_clock::time_point begin_phase0;
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
            if (!info.valid) {
                this->exausted = true;
                BOOST_LOG_TRIVIAL(debug) << "Found terminator";
                goto free_tj;
            }
            begin_time = system_clock::now();
            begin_phase0 = steady_clock::now();
            bool res = info.decode_buffer(tj_dec);
            if (!res) {
                /* The frame had some error, we just skip it (and we do not increment this->frame_num,
                 * as later stages assume that frame_num are contiguous).
                 */
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
        frame->acquisitionTime = acquisition_time;
        frame->begin_time = begin_time;
        frame->begin_phase0 = begin_phase0;
        frame->phase0();
        frame->end_phase0 = steady_clock::now();

        {
            unique_lock< mutex > lock(this->phase0_mutex);
            while (this->phase0_out != NULL) {
                if (!this->running) {
                    goto free_tj;
                }
                this->phase0_empty.wait_for(lock, seconds(1));
            }
            this->phase0_out = frame;
            this->phase0_full.notify_one();
        }
    }

    assert("Should never arrive here" == NULL);

    free_tj:
    tjDestroy(tj_dec);

}

void Context::phase1_thread() {

    BOOST_LOG_NAMED_SCOPE("phase1 thread");

    AtomicCounter counter(this->phase1_count);
    //BOOST_LOG_TRIVIAL(debug) << "Phase 1 counter: " << this->phase1_count;

    while (true) {
        // FIXME: should reorder here
        FrameAnalysis *frame;
        {
            unique_lock< mutex > lock(this->phase0_mutex);
            while (this->phase0_out == NULL) {
                if (!this->running || (this->phase0_count == 0 && this->exausted)) {
                    return;
                }
                this->phase0_full.wait_for(lock, seconds(1));
            }
            frame = this->phase0_out;
            this->phase0_out = NULL;
            this->phase0_empty.notify_one();
        }

        frame->begin_phase1 = steady_clock::now();
        frame->phase1(this->phase1_ctx);
        frame->end_phase1 = steady_clock::now();

        {
            unique_lock< mutex > lock(this->phase1_mutex);
            while (this->phase1_out != NULL) {
                if (!this->running) {
                    return;
                }
                this->phase1_empty.wait_for(lock, seconds(1));
            }
            this->phase1_out = frame;
            this->phase1_full.notify_one();
        }
    }

    assert("Should never arrive here" == NULL);

}

void Context::phase2_thread() {

    BOOST_LOG_NAMED_SCOPE("phase2 thread");

    AtomicCounter counter(this->phase2_count);

    while (true) {
        FrameAnalysis *frame;
        {
            unique_lock< mutex > lock(this->phase1_mutex);
            while (this->phase1_out == NULL) {
                if (!this->running || (this->phase1_count == 0 && this->exausted)) {
                    return;
                }
                this->phase1_full.wait_for(lock, seconds(1));
            }
            frame = this->phase1_out;
            this->phase1_out = NULL;
            this->phase1_empty.notify_one();
        }

        frame->begin_phase2 = steady_clock::now();
        frame->phase2();
        frame->end_phase2 = steady_clock::now();

        {
            unique_lock< mutex > lock(this->phase2_mutex);
            while (this->phase2_out != NULL) {
                if (!this->running) {
                    return;
                }
                this->phase2_empty.wait_for(lock, seconds(1));
            }
            this->phase2_out = frame;
            this->phase2_full.notify_one();
        }
    }

    assert("Should never arrive here" == NULL);

}

void Context::phase3_thread() {

    BOOST_LOG_NAMED_SCOPE("phase3 thread");

    AtomicCounter counter(this->phase3_count);

    while (true) {
        FrameAnalysis *frame;
        {
            unique_lock< mutex > lock(this->phase2_mutex);
            while (this->phase2_out == NULL) {
                if (!this->running || (this->phase2_count == 0 && this->exausted)) {
                    return;
                }
                this->phase2_full.wait_for(lock, seconds(1));
            }
            frame = this->phase2_out;
            this->phase2_out = NULL;
            this->phase2_empty.notify_one();
        }

        //BOOST_LOG_TRIVIAL(debug) << "frame: " << frame;
        this->reorder_buf.insert(make_pair(frame->frame_num, frame));
        frame = NULL;
        while (this->reorder_buf.begin()->first == this->phase3_frame_num) {
            frame = this->reorder_buf.begin()->second;
            this->reorder_buf.erase(this->reorder_buf.begin());

            frame->begin_phase3 = steady_clock::now();
            frame->phase3(this->phase3_ctx);
            frame->end_phase3 = steady_clock::now();
            frame->end_time = system_clock::now();

            {
                unique_lock< mutex > lock(this->phase3_mutex);
                while (this->phase3_out != NULL) {
                    if (!this->running) {
                        return;
                    }
                    this->phase3_empty.wait_for(lock, seconds(1));
                }
                this->phase3_out = frame;
                this->phase3_full.notify_one();
            }
            this->phase3_frame_num++;
        }
    }

    assert("Should never arrive here" == NULL);

}

void Context::working_thread()
{
    BOOST_LOG_NAMED_SCOPE("working thread");
    ThreadContext thread_ctx;

    while (true) {
        int frame_num;
        FrameInfo info;
        FrameSettings settings;
        system_clock::time_point acquisition_time, begin_time;
        steady_clock::time_point begin_phase0;
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
            if (!info.valid) {
                BOOST_LOG_TRIVIAL(debug) << "Found terminator";
                return;
            }
            begin_time = system_clock::now();
            begin_phase0 = steady_clock::now();
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
        frame->acquisitionTime = acquisition_time;
        frame->begin_time = begin_time;
        frame->begin_phase0 = begin_phase0;
        frame->do_things(this->frame_ctx, thread_ctx);
        frame->end_phase0 = steady_clock::now();

        {
            unique_lock< mutex > lock(this->phase3_mutex);
            while (this->phase3_out != NULL) {
                if (!this->running) {
                    return;
                }
                this->phase3_empty.wait_for(lock, seconds(1));
            }
            this->phase3_out = frame;
            this->phase3_full.notify_one();
        }
    }
}

FrameAnalysis *Context::get() {
    FrameAnalysis *frame;
    unique_lock< mutex > lock(this->phase3_mutex);
    while (this->phase3_out == NULL) {
        if (!this->running || (this->phase3_count == 0 && this->exausted)) {
            return NULL;
        }
        this->phase3_full.wait_for(lock, seconds(1));
    }
    frame = this->phase3_out;
    this->phase3_out = NULL;
    this->phase3_empty.notify_one();
    return frame;
}

void Context::wait() {
    unique_lock< mutex > lock(this->phase3_mutex);
    while (this->phase3_out == NULL) {
        if (!this->running || (this->phase3_count == 0 && this->exausted)) {
            return;
        }
        this->phase3_full.wait_for(lock, seconds(1));
    }
}

FrameAnalysis *Context::maybe_get() {
    FrameAnalysis *frame;
    unique_lock< mutex > lock(this->phase3_mutex);
    if (this->phase3_out == NULL) {
        return NULL;
    }
    frame = this->phase3_out;
    this->phase3_out = NULL;
    this->phase3_empty.notify_one();
    return frame;
}
