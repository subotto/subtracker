#ifndef FRAMEWAITER_H
#define FRAMEWAITER_H

#include <mutex>
#include <condition_variable>
#include <functional>

class FrameWaiterContext {
    friend class FrameWaiter;
private:
    std::mutex mutex;
    std::condition_variable cond;
    int frame_num;
};

class FrameWaiter
{
public:
    FrameWaiter(FrameWaiterContext &ctx, int current_frame_num, std::function<bool ()> stop_cb) :
        interrupted_by_callback(false),
        ctx(ctx),
        lock(ctx.mutex)
    {
        while (ctx.frame_num != current_frame_num) {
            if (stop_cb()) {
                this->interrupted_by_callback = true;
                return;
            }
            ctx.cond.wait_for(this->lock, std::chrono::seconds(1));
        }
        ctx.frame_num++;
    }

    ~FrameWaiter() {
        ctx.cond.notify_all();
    }

    bool interrupted_by_callback;

private:
    FrameWaiterContext &ctx;
    std::unique_lock< std::mutex > lock;
};

#endif // FRAMEWAITER_H
