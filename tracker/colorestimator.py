#!/usr/bin/env python2
# coding=utf8
import numpy


class ColorEstimator:

    def __init__(self, rectifier):
        self.rectifier = rectifier

    def estimate(self, frames, model, mean_axis):
        model.color_mean[...] = numpy.average(frames.rectification, mean_axis)
        deviation = frames.rectification - model.color_mean
        scatter = numpy.einsum("...i,...j", deviation, deviation)
        model.variance[...] = numpy.average(scatter, (0, 1, 2))
