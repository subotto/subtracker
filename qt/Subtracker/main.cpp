#include "mainwindow.h"
#include "logging.h"
#include <QApplication>

#include <chrono>
#include <thread>

#include <sys/time.h>
#include <sys/resource.h>

int main(int argc, char *argv[])
{
    setup_logging();
    BOOST_LOG_NAMED_SCOPE("main");
    BOOST_LOG_TRIVIAL(info) << "Starting Subtracker!";

    // Apparently this program may and up eating a lot of RAM and I don't want it to freeze my computer; therefore I set some limits
    struct rlimit limit;
    int res;
    limit.rlim_cur = 1024 * 1024 * 1024;  // bytes
    limit.rlim_max = limit.rlim_cur;
    res = setrlimit(RLIMIT_DATA, &limit);
    assert(res == 0);
    limit.rlim_cur = 8 * 1024 * 1024;  // bytes
    limit.rlim_max = limit.rlim_cur;
    res = setrlimit(RLIMIT_STACK, &limit);
    assert(res == 0);

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
