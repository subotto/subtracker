#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import threading
import time
import cv
import cv2
import Queue
import collections

# If would be nicer if this actually were a monotonic clock, but I see
# no way to have one in Python 2.
monotonic_time = time.time

# The two timing fields "timestamp" and "playback_time" convey
# slightly different information: the timestamp of a frame is its time
# in the video; the first frame has timestamp zero and the time
# different between two frames is meant to be a good indication of how
# much time has passed between them (for example for motion
# estimation). The playback time is the registration of the system
# clock time the frame was taken; it is not necessarily increasing by
# the same amount of the timestamp, because the system clock may be
# adjusted in the mean time. Thus, in the case of the timestamp it is
# important that the difference between two consecuitive frames is as
# close as possible os their actual time separation; while for
# playback time the important thing is that the better approximation
# of the absolute time is kept, not necessarily respecting the time
# separation with close frames.
FrameInfo = collections.namedtuple('FrameInfo', ['valid', 'timestamp', 'playback_time', 'frame'])

class FrameReader(threading.Thread):

    def __init__(self, from_file):
        self.running = True
        self.count = 0
        self.last_stats = monotonic_time()
        self.from_file = from_file
        self.queue = Queue.Queue()
        self.rate_limited = False
        self.can_drop_frames = False

    def get(self):
        try:
            frame_info = self.queue.get(block=False)
            self.queue.task_done()
            return frame_info
        except Queue.Empty:
            return None

    def stop(self):
        self.running = False
        self.join()

    def run(self):
        first_timestamp = None

        # If the input is from a camera, wait a bit to allow it to
        # initialize.
        if not self.from_file:
            time.sleep(1)

        while self.running:
            # Retrieve a frame
            retval, frame = self.cap.read()
            if not frame:
                queue.put(FrameInfo(False, None, None, None))
                return

            # Retrieve timing information: if frame is taken from a
            # file, we have to take timing for the file itself. If we
            # are rate limiting, then this already provides
            # synchronization and we can take the current time as
            # playback time. If not, we arbitrarily set playback time
            # to be the timestamp.
            if self.from_file:
                timestamp = self.cap.get(cv.CV_CAP_PROP_POS_MSEC) / 1000.0
                if self.rate_limited:
                    playback_time = time.time()
                else:
                    playback_time = timestamp

            # If the frame is taken from a camera, then there does not
            # appear to be any reliable way to have timing information
            # directly from the framework. We have to measure
            # everything, hoping that the operating system does not
            # introduce too much noise.
            else:
                playback_time = time.time()
                timestamp = monotonic_time()
                if first_timestamp is None:
                    first_timestamp = timestamp
                timestamp -= first_timestamp

            # TODO: implement rate_limited and can_drop_frames
            # TODO: implement statistics

            # Push the frame to the queue
            queue.put(FrameInfo(True, timestamp, playback_time, frame))

class CameraFrameReader(FrameReader):

    def __init__(self, device, width=320, height=240, fps=125):
        super(CameraFrameReader, self).__init__(from_file=False)

        # Open device
        self.cap = cv2.VideoCapture(device)
        if not self.cap.isOpened():
            raise Exception("Colud not initialize video capture")

        # Set width and height (TODO: will fps work? Not tested yet!)
        self.cap.set(cv.CV_CAP_PROP_FRAME_WIDTH, width)
        self.cap.set(cv.CV_CAP_PROP_FRAME_HEIGHT, height)
        self.cap.set(cv.CV_CAP_PROP_FPS, fps)

class FileFrameReader(FrameReader):

    def __init__(self, filename, simulate_live=False):
        super(FileFrameReader, self).__init__(from_file=True)

        self.cap = cv2.VideoCapture(filename)
        self.rate_limited = simulate_live
        self.can_drop_frames = simulate_live
