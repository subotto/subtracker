#!/usr/bin/env python2
# coding=utf8
import numpy
import scipy.special


class OcclusionAnalyzer:

    def __init__(self, rectifier):
        self.occluded_p = 0.1
        self.occluded_lpr = scipy.special.logit(self.occluded_p)


