#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import sys
import cv2

from control import ControlPanel
from context import SubtrackerContext
from framereader import CameraFrameReader, FileFrameReader

def feed_frames(frame_reader, ctx, panel):
    frame_num = 0
    while True:
        # Decide whether we have to update GUI; so far it always
        # happen!
        update_display = True
        if update_display:
            while True:
                c = cv2.waitKey()
                if c < 0:
                    break
                c &= 0xff
                panel.on_key_pressed(c)

        # Get a frame and feed it to the context
        frame_info = frame_reader.get()
        if not frame_info.valid:
            break
        ctx.feed(frame_info.frame, frame_info.timestamp, frame_info.playback_time)

        # Retrieve as many processed frames as possible
        while True:
            frame_analysis = ctx.get_processed_frame()
            if frame_analysis is None:
                break
            panel.on_new_analysis(frame_analysis)
            sys.stdout.write(frame_analysis.get_csv_line())

        # Prepare for next round
        frame_num += 1

def main():
    # Read arguments
    if len(sys.argv) in [3, 4]:
        video_filename = sys.argv[1]
        ref_filename = sys.argv[2]
        if len(sys.argv) == 4:
            mask_filename = sys.argv[3]
        else:
            mask_filename = None
    else:
        print >> sys.stderr, "Usage: %s <video> <reference subotto> [<reference subotto mask>]" % (sys.argv[0])
        print >> sys.stderr, "<video> can be: "
        print >> sys.stderr, "\tn (a single digit number) - live capture from video device n"
        print >> sys.stderr, "\tfilename+ (with a trailing plus) - simulate live capture from video file"
        print >> sys.stderr, "\tfilename (without a trailing plus) - batch analysis of video file"
        print >> sys.stderr, "<reference subotto> is the reference image used to look for the table"
        print >> sys.stderr, "<reference sumotto mask> is an optional B/W image, of the same size,"
        print >> sys.stderr, "\twhere black spots indicate areas of the reference image to hide"
        print >> sys.stderr, "\twhen looking for the table (such as moving parts and spurious features)"
        sys.exit(1)

    # Read input files
    ref_frame = cv2.imread(ref_filename)
    if mask_filename is not None:
        mask_frame = cv2.imread(mask_filename)
    else:
        mask_frame = None

    # Create panel and context
    panel = ControlPanel()
    ctx = SubtrackerContext()

    # Create frame reader
    if len(video_filename) == 1:
        frame_reader = CameraFrameReader(ord(video_filename) - ord('0'))
    else:
        simulate_live = False
        if video_filename[-1] == '+':
            video_filename = video_filename[:-1]
            simulate_live = True
        frame_reader = FileFrameReader(video_filename, simulate_live)
    frame_reader.start()

    # Do the actual work
    feed_frames(frame_reader, ctx, panel)

    # When finishing, wait for the reader thread to terminate
    # gracefully
    frame_reader.stop()

if __name__ == '__main__':
    main()
