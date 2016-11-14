#ifndef ATOMICCOUNTER_H
#define ATOMICCOUNTER_H

#include <atomic>

#include "logging.h"

// See some discussion here: http://stackoverflow.com/q/40498351/807307

class AtomicCounter
{
public:
    AtomicCounter(std::atomic< int > &atomic) : atomic(atomic) {
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
    }

private:
    std::atomic< int > &atomic;
};

#endif // ATOMICCOUNTER_H
