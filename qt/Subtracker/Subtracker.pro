
QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Subtracker
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    videowidget.cpp \
    framereader.cpp \
    logging.cpp \
    worker.cpp \
    context.cpp \
    frameanalysis.cpp \
    categorybutton.cpp \
    ballpanel.cpp \
    treesubframe.cpp \
    foosmenpanel.cpp \
    beginningpanel.cpp \
    memory.c \
    debugpanel.cpp \
    cv.cpp \
    coordinates.cpp \
    frameanalysis_tracking.cpp \
    spotstracker.cpp

HEADERS  += mainwindow.h \
    videowidget.h \
    framereader.h \
    logging.h \
    worker.h \
    context.h \
    frameanalysis.h \
    atomiccounter.h \
    framesettings.h \
    categorybutton.h \
    ballpanel.h \
    treesubframe.h \
    foosmenpanel.h \
    beginningpanel.h \
    memory.h \
    framewaiter.h \
    coordinates.h \
    debugpanel.h \
    cv.h \
    spotstracker.h

FORMS    += mainwindow.ui \
    ballpanel.ui \
    foosmenpanel.ui \
    beginningpanel.ui \
    debugpanel.ui

QMAKE_CXXFLAGS += -std=c++17 -march=native -DBOOST_ALL_DYN_LINK $$system(pkg-config --cflags opencv)
QMAKE_LFLAGS += -lturbojpeg -lboost_log_setup -lboost_log -lboost_system -lboost_thread $$system(pkg-config --libs opencv)

# We do not use qmake own system for interfacing with pkg-config, because I cannot make it honour PKG_CONFIG_PATH which is essential for using our own version of OpenCV
#CONFIG += link_pkgconfig
#PKGCONFIG += opencv
