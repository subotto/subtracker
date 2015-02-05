#!/usr/bin/env python2
# coding=utf8
import numpy


class Camera:

    def __init__(self, frame_rate, frame_size, channels, dtype="u1"):
        self.frame_rate = frame_rate
        self.frame_size = frame_size
        self.channels = channels
        self.dtype = numpy.dtype(dtype)

    def capture(self, cap, data):
        for i in numpy.ndindex(data.frames.shape):
            data.frames.image[i] = cap.read()[1]
            # TODO: timestamp etc.
