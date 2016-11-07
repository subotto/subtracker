#include "mainwindow.h"
#include "framereader.h"
#include "logging.h"
#include <QApplication>

#include <chrono>
#include <thread>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    setup_logging();

    BOOST_LOG_NAMED_SCOPE("main");
    BOOST_LOG_TRIVIAL(info) << "Starting Subtracker!";

    FrameCycle *f = new JPEGReader("test.gjpeg", true, true);
    f->start();

    //std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    //delete f;

    return a.exec();
}
