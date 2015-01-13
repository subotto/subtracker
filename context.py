#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import copy
import Queue
import collections
import logging

from monotonic_time import monotonic_time

from tabletracker import TableTracker, TableTrackingSettings
from spotstracker import SpotsTracker, Spot, Layer


class FrameSettings:

    def __init__(self):
        self.table_tracking_settings = TableTrackingSettings()


class FrameAnalysis:

    def __init__(self, frame, frame_num, timestamp, playback_time, frame_settings, prev_frame_analysis, controls):
        self.frame = frame
        self.frame_num = frame_num
        self.timestamp = timestamp
        self.playback_time = playback_time
        self.frame_settings = copy.deepcopy(frame_settings)
        self.controls = controls

        # Table tracker
        prev_table_tracker = prev_frame_analysis.table_tracker if prev_frame_analysis is not None else None
        self.table_tracker = TableTracker(prev_table_tracker, self.frame_settings.table_tracking_settings, controls.subpanel("table tracking"))

        # Computed data
        self.table_transform = None
        self.ball_density = None

        # Final data
        self.ball_pos = None  # False if absent, (x, y) if present
        self.bars_pos = None  # list of (lists of?) (shift, angle)

        # Timing data
        self.timings = {}

        # Record beginning of processing of this frame
        self.tic("lifetime")

    def do_table_tracking(self):
        self.tic("table tracking")
        self.table_transform = self.table_tracker.track_table(self.frame)
        # TODO: warp table
        self.toc("table tracking")

    def do_compute_ball_density(self):
        # TODO
        self.ball_density = None

    def get_csv_line(self):
        line = "%.5f" % (self.playback_time)
        if self.ball_pos:
            line += ",%.5f,%.5f" % (self.ball_pos[0], self.ball_pos[1])
        else:
            line += ",,"
        # FIXME: decide how bars are to be enumerated
        #for side in [0, 1]:
        #    for bar in xrange(BARS):
        #        line += ",%.5f,%.5f" % (self.bars_pos[side][bar][0], self.bars_pos[side][bar][1])
        line += '\n'

        # Record finishing of processing of this frame
        self.toc("lifetime")

        return line

    def tic(self, name):
        assert name not in self.timings
        self.timings[name] = [monotonic_time(), None]

    def toc(self, name):
        assert name in self.timings
        assert self.timings[name][1] is None
        self.timings[name][1] = monotonic_time()
        logging.info("Frame %d, %s took %f seconds", self.frame_num, name, self.timings[name][1] - self.timings[name][0])


class SubtrackerContext:

    def __init__(self, control_panel):
        self.last_frame_num = 0
        self.frame_settings = FrameSettings()
        self.prev_frame_analysis = None
        self.ready_frames = Queue.Queue()
        self.tracking_frames = collections.deque()
        self.spots_tracker = SpotsTracker()
        self.control_panel = control_panel

    def feed(self, frame, timestamp, playback_time):
        # Create the controls associated to a frame
        frame_controls = self.control_panel.create_frame_controls()

        # Create the FrameAnalysis object
        frame_analysis = FrameAnalysis(frame, self.last_frame_num, timestamp, playback_time, self.frame_settings, self.prev_frame_analysis, frame_controls)
        self.last_frame_num += 1

        # Do all sort of nice things to the frame
        # TODO
        frame_analysis.do_table_tracking()
        frame_analysis.do_compute_ball_density()

        # Pass the frame to the spots tracker
        # TODO
        self.tracking_frames.append(frame_analysis)
        #logging.debug(repr((frame_analysis.frame_num, frame_analysis.timestamp)))
        spots = [Spot(point, weight) for point, weight in []]
        layer = Layer(spots, frame_analysis.frame_num, frame_analysis.timestamp)
        ready_info = self.spots_tracker.push_back_and_get_info(layer)
        for ready_frame_num, ready_position in ready_info:
            ready_frame_analysis = self.tracking_frames.popleft()
            assert ready_frame_analysis.frame_num == ready_frame_num
            ready_frame_analysis.ball_pos = ready_position
            self.ready_frames.put(ready_frame_analysis)

        # Save frame analysis as previous one
        self.prev_frame_analysis = frame_analysis

    def get_processed_frame(self):
        try:
            frame_analysis = self.ready_frames.get(block=False)
            self.ready_frames.task_done()
            return frame_analysis
        except Queue.Empty:
            return None
