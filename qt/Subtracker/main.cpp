#include "mainwindow.h"
#include "framereader.h"
#include <QApplication>

#include <boost/log/core/core.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/channel_feature.hpp>
#include <boost/log/sources/channel_logger.hpp>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    boost::log::sources::channel_logger< > logger(boost::log::keywords::channel="test");
    BOOST_LOG(logger) << "Test!";

    return a.exec();
}
