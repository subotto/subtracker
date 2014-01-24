UNAME := $(shell uname -s)
CPP = g++
CFLAGS = $(shell pkg-config --cflags opencv) -g -O2
PWD := $(shell pwd)
CPPFLAGS = $(shell pkg-config --cflags opencv) -I$(PWD) -g -std=c++11 -O2
LIBS = $(shell pkg-config --libs opencv)

HEADERS = \
blobs_finder.hpp \
blobs_tracker.hpp \
framereader.hpp \
jobrunner.hpp \
median.hpp \
subotto_metrics.hpp \
subotto_tracking.hpp \
utility.hpp \
v4l2cap.hpp

OBJECTS = \
blobs_tracker.o \
jobrunner.o \
subotto_metrics.o \
subotto_tracking.o \
utility.o \
v4l2cap.o

BINARIES = \
subtracker2014

TEST_BINARIES = \
tests/camera_undistort_test \
tests/framereader_test \
tests/jobrunner_test \
tests/subotto_tracking_test

all: $(BINARIES) tests

tests: $(TEST_BINARIES)
.PHONY: tests

clean:
	rm $(OBJECTS)
	rm $(BINARIES)
	rm $(TEST_BINARIES)

%.o: %.cpp $(HEADERS) Makefile
	$(CPP) $(CPPFLAGS) -c $<

%: %.cpp $(OBJECTS) Makefile
	$(CPP) $(CPPFLAGS) -o $@ $< $(OBJECTS) $(LIBS)
