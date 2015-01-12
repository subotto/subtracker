#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import copy
import Queue

from tabletracker import TableTracker, TableTrackingSettings


class FrameSettings:

    def __init__(self):
        self.table_tracking_settings = TableTrackingSettings()


class FrameAnalysis:

    def __init__(self, frame, frame_num, timestamp, playback_time, frame_settings, prev_frame_analysis):
        self.frame = frame
        self.frame_num = frame_num
        self.timestamp = timestamp
        self.playback_time = playback_time
        self.frame_settings = copy.deepcopy(frame_settings)

        # Table tracker
        prev_table_tracker = prev_frame_analysis.table_tracker if prev_frame_analysis is not None else None
        self.table_tracker = TableTracker(prev_table_tracker, self.frame_settings.table_tracking_settings)

        # Computed data
        self.table_transform = None

        # Final data
        self.ball_pos = None  # False if absent, (x, y) if present
        self.bars_pos = None  # list of (lists of?) (shift, angle)

    def do_table_tracking(self):
        self.table_transform = self.table_tracker.track_table(self.frame)
        # TODO: warp table

    def get_csv_line(self):
        line = "%.5f" % (self.playback_time)
        if self.ball_pos:
            line += ",%.5f,%.5f" % (self.ball_pos[0], self.ball_pos[1])
        else:
            line += ",,"
        # FIXME: decide how bars are to be enumerated
        for side in [0, 1]:
            for bar in xrange(BARS):
                line += ",%.5f,%.5f" % (self.bars_pos[side][bar][0], self.bars_pos[side][bar][1])
        return line + '\n'


class SubtrackerContext:

    def __init__(self):
        self.last_frame_num = 0
        self.frame_settings = None
        self.prev_frame_analysis = None
        self.ready_frames = Queue.Queue()

    def feed(self, frame, timestamp, playback_time):
        # Create the FrameAnalysis object
        frame_analysis = FrameAnalysis(frame, self.last_frame_num, timestamp, playback_time, self.frame_settings, self.prev_frame_analysis)
        self.last_frame_num += 1

        # Do all sort of nice things to the frame
        frame_analysis.do_table_tracking()
        # TODO

        # FIXME: temporary, just for debugging
        self.queue.put(frame_analysis)

        # Save frame analysis as previous one
        self.prev_frame_analysis = frame_analysis

    def get_processed_frame(self):
        try:
            frame_analysis = self.queue.get(block=False)
            self.queue.task_done()
            return frame_analysis
        except Queue.Empty:
            return None