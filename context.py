#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import Queue
import collections
import logging
import cv2
import cv
import numpy

from monotonic_time import monotonic_time

from tabletracker import TableTracker, TableTrackingSettings
from spotstracker import SpotsTracker, Spot, Layer
from tablebg import TableBackgroundEstimationSettings, estimate_table_background
from transformation import pixels_to_rectangle, rectangle_to_region

logger = logging.getLogger("context")
timings_logger = logging.getLogger("timings")

class FrameSettings:

    def __init__(self, controls):
        self.table_tracking_settings = TableTrackingSettings()
        self.table_bg_settings = TableBackgroundEstimationSettings(controls.subpanel("tablebg"))

        self.table_frame_size = (300, 200)

class FrameAnalysis:

    def __init__(self, frame, frame_num, timestamp, playback_time, frame_settings, prev_frame_analysis, controls):
        self.frame = frame
        self.frame_num = frame_num
        self.timestamp = timestamp
        self.playback_time = playback_time
        self.frame_settings = frame_settings
        self.controls = controls

        # Table tracker
        prev_table_tracker = prev_frame_analysis.table_tracker if prev_frame_analysis is not None else None
        self.table_tracker = TableTracker(prev_table_tracker, self.frame_settings.table_tracking_settings, controls.subpanel("table tracking"))

        # Table analysis
        self.prev_table_bg_estimation = prev_frame_analysis.table_bg_estimation if prev_frame_analysis is not None else None
        self.table_bg_estimation = None

        # Computed data
        self.table_transform = None
        self.table_frame = None
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
        self.table_tracker.track_table(self.frame)
        self.table_corners = self.table_tracker.table_points
        self.toc("table tracking")
        self.tic("table warping")
        warping_proj = numpy.dot(rectangle_to_region(self.table_corners), pixels_to_rectangle(*self.frame_settings.table_frame_size))
        #logger.info("\n%r\n%r", self.table_corners, warping_proj)
        self.table_frame = cv2.warpPerspective(self.frame,
                                               warping_proj,
                                               self.frame_settings.table_frame_size,
                                               flags=cv2.INTER_LINEAR | cv2.WARP_INVERSE_MAP,
                                               borderMode=cv2.BORDER_REPLICATE)
        self.toc("table warping")
        self.controls.subpanel("table tracking").show("table frame", self.table_frame / 256.0)

    def do_compute_ball_density(self):
        self.tic("compute ball density")
        # TODO: using the original frame as a warped frame
        self.table_bg_estimation = estimate_table_background(self.prev_table_bg_estimation, self.frame, self.frame_settings.table_bg_settings, self.controls.subpanel("tablebg"))
        # TODO
        self.ball_density = None
        self.toc("compute ball density")

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
        timings_logger.debug("Frame %d, %s took %f seconds", self.frame_num, name, self.timings[name][1] - self.timings[name][0])


class SubtrackerContext:

    def __init__(self, control_panel):
        self.last_frame_num = 0
        self.prev_frame_analysis = None
        self.ready_frames = Queue.Queue()
        self.tracking_frames = collections.deque()
        self.spots_tracker = SpotsTracker()
        self.control_panel = control_panel

    def feed(self, frame, timestamp, playback_time):
        # Create the controls associated to a frame
        frame_controls = self.control_panel.create_frame_controls()

        # Initialize settings
        frame_settings = FrameSettings(frame_controls)

        # Create the FrameAnalysis object
        frame_analysis = FrameAnalysis(frame, self.last_frame_num, timestamp, playback_time, frame_settings, self.prev_frame_analysis, frame_controls)
        self.last_frame_num += 1

        # Do all sort of nice things to the frame
        # TODO
        frame_analysis.do_table_tracking()
        frame_analysis.do_compute_ball_density()

        # Pass the frame to the spots tracker
        # TODO
        self.tracking_frames.append(frame_analysis)
        #logger.debug(repr((frame_analysis.frame_num, frame_analysis.timestamp)))
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
