UNAME := $(shell uname -s)
CPP = g++
CFLAGS = `pkg-config --cflags opencv` -g -O2
CPPFLAGS = `pkg-config --cflags opencv` -g -std=c++11 -O2
LIBS = `pkg-config --libs opencv`

HEADERS = blobs_finder.hpp blobs_tracker.hpp
OBJECTS = blobs_finder.o blobs_tracker.o subtracker.o

all: subtracker

clean:
	rm -f track kalman
	rm subtracker
	rm *.o

subtracker:  $(OBJECTS)
	$(CPP) $(CPPFLAGS) $(LIBS) -o subtracker $(OBJECTS)

subotto_tracking_test: subotto_tracking_test.cpp subotto_tracking.o
	$(CPP) $(CPPFLAGS) $(LIBS) -o subotto_detector_test $< subotto_tracking.o

median_test: median_test.cpp median.o
	$(CPP) $(CPPFLAGS) $(LIBS) -o $@ $< median.o

ball_tracking_test: ball_tracking_test.cpp subotto_tracking.o ball_density.o
	$(CPP) $(CPPFLAGS) $(LIBS) -o ball_tracking_test $< subotto_tracking.o ball_density.o

ball_density_test: ball_density_test.cpp ball_density.o subotto_tracking.o
	$(CPP) $(CPPFLAGS) $(LIBS) -o ball_density_test $< ball_density.o subotto_tracking.o

%.o: %.cpp $(HEADERS) Makefile
	$(CPP) $(CPPFLAGS) -c $<

% : %.cpp
	$(CPP) $(CPPFLAGS) $(LIBS) -o $@ $<
