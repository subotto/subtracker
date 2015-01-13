#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import cv2
import numpy

class TableBackgroundEstimationSettings:

    def __init__(self, controls):
        # TODO: initial_value
        self.alpha = controls.trackbar("alpha", 0.005, 0.0, 0.1, 0.001).value
        self.var_alpha = controls.trackbar("var alpha", 0.005, 0.0, 0.1, 0.001).value

class TableBackgroundEstimation:

    def __init__(self):
        self.mean = None
        self.variance = None

def weighted_update(alpha, prev, curr):
    return (1 - alpha) * prev + alpha * curr

def initial_estimation(warped_frame, settings):
    estimation = TableBackgroundEstimation()
    
    # Our initial best estimate for the table is the first frame that
    # we see
    estimation.mean = warped_frame.copy()

    h, w, channels = warped_frame.shape

    # But this is going to be unreliable, so pixel channels can change
    # completely
    sigma = 1.0
    
    # Consider channels independent and homoskedastic, so initialize
    # the variance to a multiple of identity.
    estimation.variance = numpy.zeros((h, w, channels, channels))
    for ch in xrange(channels):
        estimation.variance[:,:,ch,ch] = sigma**2 * numpy.ones((h,w))

    return estimation

def compute_scatter(frame, mean):
    deviation = frame - mean

    # For each pixel, compute the scatter matrix of channels
    h, w, channels = frame.shape
    scatter = numpy.empty((h, w, channels, channels))
    # TODO: save half the computations as scatter is symmetric
    for ch1 in xrange(channels):
        for ch2 in xrange(channels):
            scatter[:,:,ch1,ch2] = deviation[:,:,ch1] * deviation[:,:,ch2]

    return scatter

def update_estimation(prev, warped_frame, settings):
    estimation = TableBackgroundEstimation()

    # Update the mean estimation using running avg
    estimation.mean = weighted_update(settings.alpha, prev.mean, warped_frame)

    # Estimate the variance (see wiki/Scatter_matrix)
    scatter = compute_scatter(warped_frame, estimation.mean)

    # Update the variance estimation using running avg
    estimation.variance = weighted_update(settings.var_alpha, prev.variance, scatter)

    return estimation

def estimate_table_background(prev, warped_frame, settings, controls):
    if prev is not None:
        estimation = update_estimation(prev, warped_frame, settings)
    else:
        estimation = initial_estimation(warped_frame, settings)

    controls.show("mean", estimation.mean)
    controls.show("covar B-G", estimation.variance[:,:,0,1])
    
    return estimation
