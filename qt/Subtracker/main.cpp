#include "mainwindow.h"
#include "logging.h"
#include <QApplication>

#include <chrono>
#include <thread>

int main(int argc, char *argv[])
{
    setup_logging();
    BOOST_LOG_NAMED_SCOPE("main");
    BOOST_LOG_TRIVIAL(info) << "Starting Subtracker!";

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
