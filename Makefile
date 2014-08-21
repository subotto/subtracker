CPP = g++
CFLAGS = $(shell pkg-config --cflags opencv) -g -O2
PWD := $(shell pwd)
CPPFLAGS = $(shell pkg-config --cflags opencv) -I$(PWD) -g -std=c++11 -O2
LIBS = $(shell pkg-config --libs opencv)

HEADERS = \
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
subtracker2014.o \
staging.o \
analysis.o

OBJECTS_subtracker2015 = \
subtracker2015.o \
framereader.o \
control.o \
v4l2cap.o \
context.o \
subotto_tracking.o \
subotto_metrics.o \
analysis.o \
staging.o \
blobs_tracker.o \
tracking_types.o \
spots_tracker.o \

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
subtracker2015
#subtracker2014
#tester

TEST_BINARIES = \
tests/framereader_test \
tests/jobrunner_test

all: $(BINARIES)

tests: $(TEST_BINARIES)
.PHONY: tests

clean:
	rm -f $(OBJECTS_subtracker2014)
	rm -f $(OBJECTS_subtracker2015)
	rm -f $(OBJECTS_tester)
	rm -f $(BINARIES)
	rm -f $(TEST_BINARIES)
	rm -f *.d

%.o: %.cpp $(HEADERS) Makefile
	$(CPP) $(CPPFLAGS) -c -o $@ $<

%.d: %.cpp Makefile
	$(CPP) $(CPPFLAGS) -M -o $@ $<

-include $(OBJECTS_subtracker2014:.o=.d)
-include $(OBJECTS_subtracker2015:.o=.d)

subtracker2014: $(OBJECTS_subtracker2014) Makefile
	$(CPP) $(CPPFLAGS) $(LIBS) -o $@ $(OBJECTS_subtracker2014)

subtracker2015: $(OBJECTS_subtracker2015) Makefile
	$(CPP) $(CPPFLAGS) $(LIBS) -o $@ $(OBJECTS_subtracker2015)

tester: $(OBJECTS_tester) Makefile
	$(CPP) $(CPPFLAGS) $(LIBS) -o $@ $(OBJECTS_tester)

Makefile:

