#ifndef ATOMICCOUNTER_H
#define ATOMICCOUNTER_H

#include <atomic>
#include <mutex>
#include <condition_variable>

#include "logging.h"

// See some discussion here: http://stackoverflow.com/q/40498351/807307

class AtomicCounter
{
public:
    AtomicCounter(std::atomic< int > &atomic, std::mutex &mutex, std::condition_variable &cond_var) : atomic(atomic), mutex(mutex), cond_var(cond_var) {
        BOOST_LOG_NAMED_SCOPE("AtomicCounter increment");
        int value = ++this->atomic;
        BOOST_LOG_TRIVIAL(debug) << "incremented to " << value;
        flush_log();
    }

    ~AtomicCounter() {
        BOOST_LOG_NAMED_SCOPE("AtomicCounter decrement");
        int value = --this->atomic;
        BOOST_LOG_TRIVIAL(debug) << "decremented to " << value;
        flush_log();
        BOOST_LOG_TRIVIAL(debug) << "notifying variable";
        std::unique_lock< std::mutex > lock(this->mutex);
        this->cond_var.notify_all();
        BOOST_LOG_TRIVIAL(debug) << "finished";
    }

private:
    std::atomic< int > &atomic;
    std::mutex &mutex;
    std::condition_variable &cond_var;
};

#endif // ATOMICCOUNTER_H
