#include "context.h"

using namespace std;

Context::Context(int slave_num, FrameProducer *producer) :
    running(true), frame_num(0), producer(producer), phase3_frame_num(0)
{
    this->slaves.emplace_back(&Context::phase1_thread, this);
    this->slaves.emplace_back(&Context::phase3_thread, this);
    for (int i = 0; i < slave_num; i++) {
        this->slaves.emplace_back(&Context::phase2_thread, this);
    }
}

Context::~Context() {
    this->running = false;
    for (auto &thread : this->slaves) {
        thread.join();
    }
}

void Context::phase1_thread() {

    while (this->running) {
        FrameInfo info = this->producer->get();
        FrameAnalysis *frame = new FrameAnalysis(info.data, this->frame_num++, info.time, this->settings);

        this->phase1_fn(frame);

        {
            unique_lock< mutex > lock(this->phase1_mutex);
            while (this->phase1_out != NULL) {
                this->phase1_empty.wait(lock);
            }
            this->phase1_out = frame;
            this->phase1_full.notify_one();
        }
    }

}

void Context::phase2_thread() {

    while (this->running) {
        FrameAnalysis *frame;
        {
            unique_lock< mutex > lock(this->phase1_mutex);
            while (this->phase1_out == NULL) {
                this->phase1_full.wait(lock);
            }
            frame = this->phase1_out;
            this->phase1_out = NULL;
            this->phase1_empty.notify_one();
        }

        this->phase2_fn(frame);

        {
            unique_lock< mutex > lock(this->phase2_mutex);
            while (this->phase2_out != NULL) {
                this->phase2_empty.wait(lock);
            }
            this->phase2_out = frame;
            this->phase2_full.notify_one();
        }
    }

}

void Context::phase3_thread() {

    while (this->running) {
        FrameAnalysis *frame;
        {
            unique_lock< mutex > lock(this->phase2_mutex);
            while (this->phase2_out == NULL) {
                this->phase2_full.wait(lock);
            }
            frame = this->phase2_out;
            this->phase2_out = NULL;
            this->phase2_empty.notify_one();
        }

        this->reorder_buf.insert(make_pair(frame->frame_num, frame));
        frame = NULL;
        while (this->reorder_buf.begin()->first == this->phase3_frame_num) {
            frame = this->reorder_buf.begin()->second;
            this->reorder_buf.erase(this->reorder_buf.begin());
            this->phase3_fn(frame);

            {
                unique_lock< mutex > lock(this->phase3_mutex);
                while (this->phase3_out != NULL) {
                    this->phase3_empty.wait(lock);
                }
                this->phase3_out = frame;
                this->phase3_full.notify_one();
            }
            this->phase3_frame_num++;
        }
    }

}

FrameAnalysis *Context::get() {
    FrameAnalysis *frame;
    unique_lock< mutex > lock(this->phase3_mutex);
    while (this->phase3_out == NULL) {
        this->phase3_full.wait(lock);
    }
    frame = this->phase3_out;
    this->phase3_out = NULL;
    this->phase3_empty.notify_one();
    return frame;
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

void Context::phase1_fn(FrameAnalysis *frame) {
    frame->phase1();
}

void Context::phase2_fn(FrameAnalysis *frame) {
    frame->phase2();
}

void Context::phase3_fn(FrameAnalysis *frame) {
    frame->phase3();
}
