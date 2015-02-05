#!/usr/bin/env python2
# coding=utf8
import numpy
import scipy
import scipy.ndimage
import scipy.special

from colorestimator import ColorEstimator
from occlusion import OcclusionAnalyzer
from pixelclassifier import PixelColorAnalyzer


class BallAnalyzer:

    def __init__(self, table, rectifier):
        self.occlusion_analyzer = OcclusionAnalyzer(rectifier)
        self.color_analyzer = PixelColorAnalyzer(rectifier)

        self.color_mean = table.ball.color
        self.variance = 0.05 ** 2 * numpy.eye(3) + 0.1 ** 2 * numpy.ones((3, 3))

        # FIXME: a lot of common code with foosmen!

        self.silhouette_size = (table.ball.diameter, table.ball.diameter)
        self.silhouette_area = self.silhouette_size[0] * self.silhouette_size[1]

        self.filter_size = tuple(s * rectifier.resolution for s in self.silhouette_size)

        self.visible_p = 0.999
        self.visible_lpr = scipy.special.logit(self.visible_p)

        w, h = rectifier.image_size

        self.dtype = [
            ("color_analysis", self.color_analyzer.analysis_dtype),
            ("present_ll_c", "f4", (h, w)),
            ("absent_ll_c", "f4", (h, w)),
            ("present_llr", "f4", (h, w)),
            ("location_llr", "f4", (h, w)),
        ]

    def analyze_color(self, data):
        self.color_analyzer.compute_ll(data.frames, self, data.frames.ball.color_analysis)

    def compute_visible_llr(self, data):
        background = data.frames.background
        ball = data.frames.ball

        ball.present_ll_c[...] = numpy.logaddexp(ball.color_analysis.ll, self.occlusion_analyzer.occluded_lpr)
        ball.absent_ll_c[...] = data.background.q_estimation * numpy.logaddexp(background.color_analysis.ll, self.occlusion_analyzer.occluded_lpr)

        ball.present_llr[...] = numpy.logaddexp(ball.present_ll_c - ball.absent_ll_c + self.visible_lpr, ball.absent_ll_c)

    def compute_location_llr(self, data):
        ball = data.frames.ball

        fw, fh = self.filter_size
        filter_size = (1, fh, fw)
        # TODO: we should multiply by (self.silhouette_area/data.frames.pixel_density)
        ball.location_llr[...] = scipy.ndimage.uniform_filter(ball.present_llr, filter_size)

