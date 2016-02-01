PWD := $(shell pwd)
PKG_CONFIG_PATH = $(PWD)/opencv_install/lib/pkgconfig
CXX = g++
CXXFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags opencv) -I$(PWD) -g -std=c++14 -march=native -O3
LIBS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs opencv) -lpthread -lturbojpeg -lboost_system

HEADERS = \
blobs_tracker.hpp \
control.hpp \
framereader.hpp \
jobrunner.hpp \
subotto_metrics.hpp \
subotto_tracking.hpp \
utility.hpp \
v4l2cap.hpp \
jpegreader.hpp \

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
analysis.o \

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
jpegreader.o \
utility.o \

OBJECTS_camera_source = \
camera_source.o \
framereader.o \
control.o \
v4l2cap.o \

OBJECTS_viewer = \
viewer.o \
framereader.o \
control.o \
v4l2cap.o \
jpegreader.o \
utility.o \

OBJECTS_tester = \
tester.o \
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
jpegreader.o \
utility.o \

BINARIES = \
subtracker2015 \
camera_source \
viewer \
tester \
#subtracker2014 \

TEST_BINARIES = \
tests/framereader_test \
tests/jobrunner_test \

all: $(BINARIES)

tests: $(TEST_BINARIES)
.PHONY: tests

clean:
	rm -f $(OBJECTS_subtracker2014)
	rm -f $(OBJECTS_subtracker2015)
	rm -f $(OBJECTS_camera_source)
	rm -f $(OBJECTS_tester)
	rm -f $(BINARIES)
	rm -f $(TEST_BINARIES)
	rm -f *.d

%.o: %.cpp $(HEADERS) Makefile
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.d: %.cpp Makefile
	$(CXX) $(CXXFLAGS) -M -o $@ $<

-include $(OBJECTS_subtracker2014:.o=.d)
-include $(OBJECTS_subtracker2015:.o=.d)

subtracker2014: $(OBJECTS_subtracker2014) Makefile
	$(CXX) $(CXXFLAGS) $(LIBS) -o $@ $(OBJECTS_subtracker2014)

subtracker2015: $(OBJECTS_subtracker2015) Makefile
	$(CXX) $(CXXFLAGS)  $(LIBS) -o $@ $(OBJECTS_subtracker2015)

camera_source: $(OBJECTS_camera_source) Makefile
	$(CXX) $(CXXFLAGS) $(LIBS) -o $@ $(OBJECTS_camera_source)

viewer: $(OBJECTS_viewer) Makefile
	$(CXX) $(CXXFLAGS) $(LIBS) -o $@ $(OBJECTS_viewer)

tester: $(OBJECTS_tester) Makefile
	$(CXX) $(CXXFLAGS) $(LIBS) -o $@ $(OBJECTS_tester)

Makefile:

