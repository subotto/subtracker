#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import cv2
import numpy

class FrameTableAnalysis:

    def __init__(self):
        # Log-likelihood of each pixel to be coming from the table background
        self.background_ll = None
        # Log-likelihood of each pixel to be coming from the ball
        self.ball_ll = None

        self.ballness = None


# The following anlyzer uses the variance matrix, but it still has to
# be tested. We use a simpler version instead.
def analyze_table_background_full(bg_estimation, warped_frame, controls):
    analysis = FrameTableAnalysis()

    # Compute multivariate normal PDF (see
    # wikipedia:Multivariate normal distribution).  We are exploiting
    # numpy broadcasting rules to do it pixel-by-pixel.

    h, w, channels = warped_frame.shape
    k = channels * 1.0

    x = warped_frame
    mu = bg_estimation.mean
    sigma = bg_estimation.variance

    term3 =  -1/2.0 * numpy.einsum("...i,...ij,...j", x-mu, numpy.linalg.inv(sigma), x-mu)
    density = -k/2 * numpy.log(2 * numpy.pi) - 1/2.0 * numpy.log(numpy.linalg.det(sigma)) + term3

    analysis.background_ll = density

    controls.show("background_ll", analysis.background_ll)

    return analysis

# Simpler version that uses the squared deviation normalized by a constant
def analyze_table_background(bg_estimation, warped_frame, controls):
    analysis = FrameTableAnalysis()

    deviation = warped_frame / 255.0 - bg_estimation.mean / 255.0

    bg_sigma = controls.trackbar("sigma", 0.2, 0.01, 0.5, 0.01).value
    analysis.background_ll = -0.5 * numpy.sum(deviation * deviation, 2) / bg_sigma**2

    controls.show("background_ll", analysis.background_ll)

    ball_intensity = controls.trackbar("ball intensity", 0.95).value
    ball_sigma = controls.trackbar("ball sigma", 0.15).value

    ball_deviation = warped_frame / 255.0 - ball_intensity * numpy.ones(3)
    analysis.ball_ll = -0.5 * numpy.sum(ball_deviation * ball_deviation, 2) / ball_sigma**2

    controls.show("ball_ll", analysis.ball_ll)

    combined_ll = analysis.ball_ll - analysis.background_ll

    analysis.ballness = cv2.blur(combined_ll, (3,3))
    controls.show("ballness", analysis.ballness)

    return analysis
