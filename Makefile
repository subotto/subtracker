UNAME := $(shell uname -s)
CPP = g++
CFLAGS = $(shell pkg-config --cflags opencv) -g -O2
CPPFLAGS = $(shell pkg-config --cflags opencv) -g -std=c++11 -O2
LIBS = $(shell pkg-config --libs opencv)

HEADERS = blobs_finder.hpp blobs_tracker.hpp utility.hpp
OBJECTS = blobs_finder.o blobs_tracker.o subtracker.o utility.o

all: subtracker

clean:
	rm -f track kalman
	rm subtracker
	rm *.o

subtracker:  $(OBJECTS)
	$(CPP) $(CPPFLAGS) -o subtracker $(OBJECTS) $(LIBS)

OBJECTS_2014 = subtracker2014.o subotto_tracking.o ball_density.o utility.o subotto_metrics.o blobs_tracker.o

subtracker2014: $(OBJECTS_2014)
	$(CPP) $(CPPFLAGS) -o $@ $(OBJECTS_2014) $(LIBS)

subotto_tracking_test: subotto_tracking_test.cpp utility.o subotto_tracking.o subotto_metrics.o
	$(CPP) $(CPPFLAGS) -o subotto_tracking_test $< utility.o subotto_tracking.o subotto_metrics.o $(LIBS)

median_test: median_test.cpp median.o
	$(CPP) $(CPPFLAGS) -o $@ $< median.o $(LIBS)

jobrunner_test: jobrunner_test.cpp jobrunner.o jobrunner.hpp
	$(CPP) $(CPPFLAGS) -o $@ $< jobrunner.o $(LIBS)

ball_tracking_test: ball_tracking_test.cpp subotto_tracking.o ball_density.o utility.o subotto_metrics.o
	$(CPP) $(CPPFLAGS) -o ball_tracking_test $< subotto_tracking.o ball_density.o utility.o subotto_metrics.o $(LIBS)

ball_tracking_giove: ball_tracking_giove.cpp subotto_tracking.o ball_density.o utility.o subotto_metrics.o blobs_tracker.o
	$(CPP) $(CPPFLAGS) -o ball_tracking_giove $< subotto_tracking.o ball_density.o utility.o subotto_metrics.o blobs_tracker.o $(LIBS)

ball_density_test: ball_density_test.cpp ball_density.o subotto_tracking.o utility.o subotto_metrics.o
	$(CPP) $(CPPFLAGS) -o ball_density_test $< ball_density.o subotto_tracking.o subotto_metrics.o $(LIBS)

mock: mock.cpp
	$(CPP) $< -std=c++11 -o mock

foosmen_tracking_test: foosmen_tracking_test.cpp ball_density.o subotto_tracking.o utility.o subotto_metrics.o
	$(CPP) $(CPPFLAGS) -o foosmen_tracking_test $< ball_density.o subotto_tracking.o utility.o subotto_metrics.o $(LIBS)

camera_undistort_test: camera_undistort_test.cpp subotto_tracking.o subotto_metrics.o utility.o
	$(CPP) $(CPPFLAGS) -o camera_undistort_test $< subotto_tracking.o subotto_metrics.o utility.o $(LIBS)

%.o: %.cpp $(HEADERS) Makefile
	$(CPP) $(CPPFLAGS) -c $<

% : %.cpp
	$(CPP) $(CPPFLAGS) -o $@ $< $(LIBS)
