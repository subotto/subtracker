#!/usr/bin/env python2
# coding=utf8
from colorestimator import ColorEstimator
from pixelclassifier import PixelColorAnalyzer


class BackgroundAnalyzer:

    def __init__(self, table, rectifier):
        self.color_estimator = ColorEstimator(rectifier)
        self.color_analyzer = PixelColorAnalyzer(rectifier)

        self.dtype = [
            ("color_analysis", self.color_analyzer.analysis_dtype),
        ]

    def estimate_color(self, data):
        self.color_estimator.estimate(data.frames, data.background, mean_axis=0)
        data.background.q_estimation = 0.8

    def analyze_color(self, data):
        self.color_analyzer.compute_ll(data.frames, data.background, data.frames.background.color_analysis)
