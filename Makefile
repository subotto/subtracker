CPP = g++
CFLAGS = $(shell pkg-config --cflags opencv) -g -O2
PWD := $(shell pwd)
CPPFLAGS = $(shell pkg-config --cflags opencv) -I$(PWD) -g -std=c++11 -O2
LIBS = $(shell pkg-config --libs opencv)

HEADERS = \
blobs_finder.hpp \
blobs_tracker.hpp \
control.hpp \
framereader.hpp \
jobrunner.hpp \
subotto_metrics.hpp \
subotto_tracking.hpp \
utility.hpp \
v4l2cap.hpp

OBJECTS_subtracker2014 = \
blobs_tracker.o \
control.o \
framereader.o \
jobrunner.o \
subotto_metrics.o \
subotto_tracking.o \
utility.o \
v4l2cap.o \
subtracker2014.o

OBJECTS_tester = \
blobs_tracker.o \
control.o \
framereader.o \
jobrunner.o \
subotto_metrics.o \
subotto_tracking.o \
utility.o \
v4l2cap.o \
tester.o

BINARIES = \
subtracker2014 \
tester

TEST_BINARIES = \
tests/framereader_test \
tests/jobrunner_test

all: $(BINARIES)

tests: $(TEST_BINARIES)
.PHONY: tests

clean:
	rm -f $(OBJECTS_subtracker2014)
	rm -f $(OBJECTS_tester)
	rm -f $(BINARIES)
	rm -f $(TEST_BINARIES)

%.o: %.cpp $(HEADERS) Makefile
	$(CPP) $(CPPFLAGS) -c $<

subtracker2014: $(OBJECTS_subtracker2014) Makefile
	$(CPP) $(CPPFLAGS) $(LIBS) -o $@ $(OBJECTS_subtracker2014)

tester: $(OBJECTS_tester) Makefile
	$(CPP) $(CPPFLAGS) $(LIBS) -o $@ $(OBJECTS_tester)

Makefile:

