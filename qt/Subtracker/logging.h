#ifndef LOGGING_H
#define LOGGING_H

#include <boost/log/trivial.hpp>
#include <boost/log/attributes/named_scope.hpp>

void setup_logging();
void flush_log();

#endif // LOGGING_H
