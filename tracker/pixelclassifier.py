#!/usr/bin/env python2
# coding=utf8
import numpy


class PixelColorAnalyzer:

    def __init__(self, rectifier):
        self.rectifier = rectifier

        c = rectifier.channels
        rw, rh = rectifier.image_size

        self.analysis_dtype = [
            ("deviation", "f4", (rh, rw, c)),
            ("m_distance_2", "f4", (rh, rw)),
            ("ll", "f4", (rh, rw)),
        ]

    def compute_ll(self, frames, model, output):
        k = self.rectifier.channels

        precision = numpy.linalg.inv(model.variance)

        # compute the PDF of a Multivariate Normal Distribution

        output.deviation[...] = frames.rectification - model.color_mean
        output.m_distance_2[...] = numpy.einsum("...i,...ij,...j", output.deviation, precision, output.deviation)
        output.ll[...] = -0.5 * (k * numpy.log(2 * numpy.pi) + numpy.log(numpy.linalg.det(model.variance)) + output.m_distance_2)
