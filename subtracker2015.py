#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import sys
import cv2
import logging

from control import ControlPanel
from context import SubtrackerContext
from framereader import CameraFrameReader, FileFrameReader

logger = logging.getLogger("main program")


# Mock implementation
class MockControlPanel():

    def on_key_pressed(self, c):
        pass

    def on_new_analysis(self, frame_analysis):
        pass


def feed_frames(frame_reader, ctx, panel):
    frame_num = 0
    update_gui_skip = 1
    step_frame = False
    step_on_frame_produced = False
    while True:
        # Decide whether we have to update GUI; so far it always
        # happen!
        update_display = (frame_num % (update_gui_skip + 1) == 0) or step_frame
        if update_display:
            while True:
                #logger.debug("Before waitKey()")
                c = cv2.waitKey(1)
                if c < 0:
                    if not step_frame:
                        break
                    else:
                        continue
                c &= 0xff
                c = chr(c)
                #logger.debug("After waitKey() = %r", c)
                if step_frame and c == '.':
                    break
                if c == 'q':
                    raise KeyboardInterrupt
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
            if step_on_frame_produced:
                step_frame = True

        # Prepare for next round
        frame_num += 1

def main():
    logging.basicConfig(level=logging.INFO,
                        format='%(asctime)s.%(msecs).03d (%(name)s) %(message)s',
                        datefmt='%d/%m/%Y %H:%M:%S')

    # Read arguments
    if len(sys.argv) in [1,2]:
        video_filename = sys.argv[1]
    else:
        print >> sys.stderr, "Usage: %s <video>" % (sys.argv[0])
        print >> sys.stderr, "<video> can be: "
        print >> sys.stderr, "\tn (a single digit number) - live capture from video device n"
        print >> sys.stderr, "\tfilename+ (with a trailing plus) - simulate live capture from video file"
        print >> sys.stderr, "\tfilename (without a trailing plus) - batch analysis of video file"
        sys.exit(1)

    # Create panel and context
    panel = ControlPanel()
    ctx = SubtrackerContext(panel)

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
    try:
        feed_frames(frame_reader, ctx, panel)
    except KeyboardInterrupt:
        logger.info("Exit requested")
    except:
        logger.info("Exit because of exception", exc_info=True)

    # When finishing, wait for the reader thread to terminate
    # gracefully and consume left frames
    frame_reader.stop()
    while True:
        if frame_reader.get(block=False) is None:
            break
    frame_reader.join()

    logger.info("Exiting")

if __name__ == '__main__':
    main()
