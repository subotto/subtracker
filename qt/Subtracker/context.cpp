#include "context.h"

Context::Context(int slave_num)
{
    for (int i = 0; i < slave_num; i++) {
        this->slaves.emplace_back(&Context::slave_fn, this, i);
    }
}

void Context::slave_fn(int id) {

}
