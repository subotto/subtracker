#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import math
import threading
import urllib2
import time
import json
import collections

import pygame
import pygame.locals

from OpenGL.GL import *
from OpenGL.GLU import *

from metrics import *
from objlib import read_obj

from monotonic_time import monotonic_time

#REQUEST_URL = 'http://uz.sns.it/24ore/tracking.json?last_timestamp=%(last_timestamp)f&convert_units=0'
REQUEST_URL = 'http://localhost:8000/?last_timestamp=%(last_timestamp)f&convert_units=0'
REQUEST_TIMEOUT = 2.0
REQUEST_SLEEP = 0.5

FADING_FACTOR = 0.1

class QueueLengthEstimator:

    def __init__(self):
        self.average = None
        self.variance = None

        # Our first bet on average chunk size is the time between two
        # requests
        self.chunk_average = REQUEST_SLEEP

    def parse_frames(self, frames, ref_time, last_timestamp):
        if len(frames) == 0:
            return
        if last_timestamp is None:
            last_timestamp = frames[0]['timestamp']
        chunk_len = frames[-1]['timestamp'] - last_timestamp

        # if self.average is None:
        #     for frame in frames:
        #         self.average = (frame['timestamp'] - ref_time) / float(len(frames))
        #     for frame in frames:
        #         self.variance = (frame['timestamp'] - self.average) / float(len(frames))
        # else:
        #     for frame in frames:
        #         self.average = FADING_FACTOR * (frame['timestamp'] - ref_time) + (1.0 - FADING_FACTOR) * self.average
        #         self.variance = FADING_FACTOR * ((frame['timestamp'] - self.average) ** 2) + (1.0 - FADING_FACTOR) * self.variance

        if self.chunk_average is None:
            self.chunk_average = chunk_len
        else:
            self.chunk_average = FADING_FACTOR * (chunk_len) + (1.0 - FADING_FACTOR) * self.chunk_average

    def get_length_estimate(self):
        if self.chunk_average is None:
            return 2.0
        return 2.0 * self.chunk_average

class RequestThread(threading.Thread):

    def __init__(self):
        threading.Thread.__init__(self)
        self.stop = False
        self.last_timestamp = None
        self.daemon = True
        self.frames = collections.deque()
        self.qle = QueueLengthEstimator()

    def run(self):
        first_time = True
        while not self.stop:

            # Wait for some time (but not the first time)
            if not first_time:
                time.sleep(REQUEST_SLEEP)
            else:
                first_time = False

            # Perform the HTTP request
            try:
                response = urllib2.urlopen(REQUEST_URL % {'last_timestamp': self.last_timestamp if self.last_timestamp is not None else 0.0},
                                           timeout=REQUEST_TIMEOUT)
                ref_time = monotonic_time()
            except urllib2.URLError:
                print "Request failed or timed out"
                continue

            # Load JSON data
            try:
                data = json.load(response)
            except:
                print "Could not parse JSON"
                continue

            # Compute timing data and push data in the queue
            try:
                self.qle.parse_frames(data['data'], ref_time, self.last_timestamp)
                for frame in data['data']:
                    self.frames.append(frame)
                if len(data['data']) > 0:
                    self.last_timestamp = data['data'][-1]['timestamp']
            except:
                print "Malformed JSON data"
                raise
                continue

            print "Last timestamp: %f" % (self.last_timestamp)

def resize(width, height):

    # Update pygame
    pygame.display.set_mode((width, height), pygame.OPENGL | pygame.DOUBLEBUF | pygame.HWSURFACE | pygame.RESIZABLE)

    # Use new viewport
    glViewport(0, 0, width, height)

    # Set up projection
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    gluPerspective(60.0, float(width)/float(height), 0.1, 500.0)

    # Set up trivial model view
    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()

def render(time):

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()
    gluLookAt(0.8 * math.cos(time / 10.0), 0.8 * math.sin(time / 10.0), 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0)
    #gluLookAt(0.0, -0.8, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0)

    glLight(GL_LIGHT0, GL_POSITION, (0.4, 0.6, 1.2))

class FramePicker:

    WARP_COEFF = 0.1
    MAX_WARP_OFFSET = 0.2
    MAX_SKIP = 1.0

    def __init__(self, queue, qle):
        self.queue = queue
        self.qle = qle

        # Frame time is the timestamp of the frame that is being
        # played now (i.e., in the time reference of the server); it
        # can be None, meaning that playback is paused at the moment
        self.frame_time = None

        # Real time is the current system (monotonic) time, used to
        # estimate how much time was spent between a call of
        # pick_frame() and another
        self.real_time = None

    def pick_frame(self):
        # Measure how much time was elapsed
        new_time = monotonic_time()
        if self.real_time is None:
            self.real_time = new_time
        elapsed = new_time - self.real_time
        self.real_time = new_time

        # Estimate the target queue length, compare it with the actual
        # length and decide the time warping factor (FIXME: actual_len
        # is slightly wrong, it implicitly discards one frame)
        target_len = self.qle.get_length_estimate()
        if len(self.queue) > 0:
            actual_len = self.queue[-1]["timestamp"] - self.queue[0]["timestamp"]
        else:
            actual_len = 0.0
        warping = 1.0 + FramePicker.WARP_COEFF * float(actual_len - target_len) / target_len
        if warping < 1.0 - FramePicker.MAX_WARP_OFFSET:
            warping = 1.0 - FramePicker.MAX_WARP_OFFSET
        if warping > 1.0 + FramePicker.MAX_WARP_OFFSET:
            warping = 1.0 + FramePicker.MAX_WARP_OFFSET

        # If we have a lock, adjust the frame time and take the
        # appropriate frame
        if self.frame_time is not None:
            self.frame_time += warping * elapsed

        # If not, directly aim to the appropriate queue length
        else:
            # If we have enough buffer, we just cut the excess part;
            # if not, we better wait to receive some, so we leave
            # frame_time to None
            if actual_len >= target_len:
                self.frame_time = self.queue[-1]["timestamp"] - target_len

        # Discard frames until we arrive at frame_time: if we run out
        # of frames we temporarily suspend playback
        if self.frame_time is not None:
            while True:
                try:
                    frame = self.queue[0]
                except IndexError:
                    self.frame_time = None
                    ret = None
                    print "Queue empty!"
                    break

                if self.frame_time < frame["timestamp"]:
                    ret = frame
                    break

                self.queue.popleft()

        else:
            ret = None

        # If the selected frame is too much in the future, make a time
        # jump (otherwise there will be a static frame sitting on the
        # screen for a lot of time)
        if ret is not None and ret["timestamp"] > self.frame_time + FramePicker.MAX_SKIP:
            self.frame_time = ret["timestamp"]

        print "Queue length: %f (%d), target length: %f, warping: %f, frame time: %s, frame timestamp: %s" % (actual_len, len(self.queue), target_len, warping, self.frame_time, ret["timestamp"] if ret is not None else None)

        return ret

fps = 30.0

def main():

    # Spawn request thread and setup frame picker
    thread = RequestThread()
    picker = FramePicker(thread.frames, thread.qle)
    thread.start()

    # Initialize pygame stuff
    pygame.init()
    clock = pygame.time.Clock()
    resize(640, 480)
    pygame.display.set_caption('Subotto tracker viewer')

    # Initialize OpenGL stuff
    glEnable(GL_DEPTH_TEST)
    glShadeModel(GL_FLAT)
    glClearColor(0.0, 0.0, 0.0, 0.0)
    glEnable(GL_LIGHTING)
    glEnable(GL_LIGHT0)
    glLight(GL_LIGHT0, GL_DIFFUSE, (0.8, 0.8, 0.8))
    glLight(GL_LIGHT0, GL_AMBIENT, (0.1, 0.1, 0.1))

    objects = read_obj('omino.obj')
    current_frame = None

    frame = 0
    while True:

        # Receive and process events
        for event in pygame.event.get():

            if event.type == pygame.locals.QUIT:
                sys.exit(1)

            if event.type == pygame.locals.VIDEORESIZE:
                resize(event.w, event.h)

        new_frame = picker.pick_frame()
        buffering = False
        if new_frame is not None:
            buffering = True
            current_frame = new_frame

        render(frame / fps)

        objects['Campo'].draw()

        counters = [0, 0]
        if current_frame is not None:

            for i, rod in enumerate(ROD_CONFIGURATION):
                foosmen_num, foosmen_dist, team = rod
                color = ROD_COLORS[team]
                rod_str = 'rod_%s_%d' % (color, counters[team])
                counters[team] += 1
                angle_str = '%s_angle' % (rod_str)
                shift_str = '%s_shift' % (rod_str)

                glPushMatrix()
                glTranslate((i - float(ROD_NUMBER-1) / 2.0) * ROD_DISTANCE, 0.0, ROD_HEIGHT)

                glPushMatrix()
                glRotate(90.0, 1.0, 0.0, 0.0)
                glScale(ROD_DIAMETER * CYLINDER_FACTOR, ROD_DIAMETER * CYLINDER_FACTOR, FIELD_HEIGHT * CYLINDER_FACTOR)
                objects['Stecca'].draw()
                glPopMatrix()

                if current_frame[shift_str] is not None:
                    glTranslate(0.0, current_frame[shift_str], 0.0)
                if current_frame[angle_str] is not None:
                    glRotate(-90.0 / math.pi * current_frame[angle_str], 0.0, 1.0, 0.0)

                for j in xrange(foosmen_num):
                    glPushMatrix()
                    glTranslate(0.0, (j - float(foosmen_num - 1) / 2.0) * foosmen_dist, 0.0)
                    if team == 0:
                        objects['Omino_rosso'].draw()
                    else:
                        objects['Omino_blu'].draw()
                    glPopMatrix()
                glPopMatrix()

            if current_frame['ball_x'] is not None and current_frame['ball_y'] is not None:
                glPushMatrix()
                glTranslate(current_frame['ball_x'],
                            current_frame['ball_y'],
                            0.0)
                objects['Pallina'].draw()
                glPopMatrix()

            #print time_delta, current_frame['ball_x'], current_frame['ball_y']

        pygame.display.flip()
        clock.tick(fps)
        #print clock.get_fps()
        frame += 1

if __name__ == '__main__':
    main()
