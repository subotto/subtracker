#ifndef LOGGING_H
#define LOGGING_H

#include <boost/log/trivial.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/expressions.hpp>

void setup_logging(boost::log::trivial::severity_level min_severity = boost::log::trivial::info);
void flush_log();

#endif // LOGGING_H
