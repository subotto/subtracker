#!/usr/bin/env python2
# coding=utf8
import cv2
import numpy
import scipy.ndimage
import scipy.special

from colorestimator import ColorEstimator
from occlusion import OcclusionAnalyzer
from pixelclassifier import PixelColorAnalyzer
from table import Foosman
from transformation import get_image_transform


class TeamFoosmenAnalyzer:

    def __init__(self, table, rectifier):
        self.visible_p = 0.4
        self.visible_lpr = scipy.special.logit(self.visible_p)

        self.silhouette_size = (2 * table.foosmen.feet_height, table.foosmen.width)
        self.silhouette_area = self.silhouette_size[0] * self.silhouette_size[1]

        self.filter_size = tuple(s * rectifier.resolution for s in self.silhouette_size)

        self.color_estimator = ColorEstimator(rectifier)
        self.color_analyzer = PixelColorAnalyzer(rectifier)
        self.occlusion_analyzer = OcclusionAnalyzer(rectifier)

        w, h = rectifier.image_size

        self.dtype = [
            ("color_analysis", self.color_analyzer.analysis_dtype),
            ("present_ll_c", "f4", (h, w)),
            ("absent_ll_c", "f4", (h, w)),
            ("present_llr", "f4", (h, w)),
            ("location_llr", "f4", (h, w)),
        ]

    def initialize_model(self, team, data):
        model = data.team_foosmen[team.index]
        model.color_mean[...] = team.foosmen.color
        model.variance[...] = 0.1 ** 2 * numpy.eye(3) + 0.1 ** 2 * numpy.ones((3, 3))

    def analyze_color(self, team, data):
        i = team.index
        self.color_analyzer.compute_ll(data.frames, data.team_foosmen[i], data.frames.team_foosmen.color_analysis[:, i])

    def compute_visible_llr(self, team, data):
        background = data.frames.background
        team_foosmen = data.frames.team_foosmen[:, team.index].view(numpy.recarray)

        team_foosmen.present_ll_c[...] = numpy.logaddexp(team_foosmen.color_analysis.ll, self.occlusion_analyzer.occluded_lpr)
        team_foosmen.absent_ll_c[...] = data.background.q_estimation * numpy.logaddexp(background.color_analysis.ll, self.occlusion_analyzer.occluded_lpr)

        team_foosmen.present_llr[...] = numpy.logaddexp(team_foosmen.present_ll_c - team_foosmen.absent_ll_c + self.visible_lpr, team_foosmen.absent_ll_c)

    def compute_location_llr(self, team, data):
        team_foosmen = data.frames.team_foosmen[:, team.index].view(numpy.recarray)

        fw, fh = self.filter_size
        filter_size = (1, fh, fw)
        # TODO: we should multiply by (self.silhouette_area/data.frames.pixel_density)
        team_foosmen.location_llr[...] = scipy.ndimage.uniform_filter(team_foosmen.present_llr, filter_size)


class RodAnalyzer:

    def __init__(self, table, rod, rectifier):
        self.table = table
        self.rod = rod
        self.rectifier = rectifier

        self.shift_max = rod.type.shift_max
        self.translation_resolution = 80  # pixel/meter

        # Center location moves only along the Y axis,
        # but we approximate this by allowing a small horizontal movement
        # so that transform matrices are not singular and units of measurement
        # make sense more easily
        self.location_delta_width = 0.02

        self.shift_size = int(2 * self.shift_max * self.translation_resolution)

        h = self.shift_size

        # Transforms the translations into the center location
        self.shift_transform = get_image_transform((self.location_delta_width, 2 * self.shift_max), (1, h))

        self.foosman_analyzer = [FoosmanAnalyzer(table, foosman, self) for foosman in rod.foosmen]

        self.dtype = [
            ("shift_llr", "f8", h),
            ("foosman", [
                ("shift_llr", "f8", h),
            ], len(rod.foosmen))
        ]

    def compute_shift_llr(self, data):
        pass


class FoosmanAnalyzer:

    def __init__(self, table, foosman, rod_analyzer):
        self.table = table
        self.foosman = foosman
        self.rod_analyzer = rod_analyzer

        self.location_transform = numpy.einsum("...ij,...jk,...kl",
            rod_analyzer.rectifier.transform,
            foosman.transform,
            numpy.linalg.inv(rod_analyzer.shift_transform)
        )

    def compute_shift_llr(self, data):
        team_foosmen = data.frames.team_foosmen[:, self.foosman.rod.team.index]
        foosman_data = data.frames.rod.field(self.foosman.rod.index).foosman[:, self.foosman.index]

        h = self.rod_analyzer.shift_size

        for f in xrange(len(data.frames)):
            llr = team_foosmen.location_llr[f]
            foosman_data.shift_llr[f] = cv2.warpPerspective(llr, self.location_transform, (1, h), None, cv2.WARP_INVERSE_MAP)[:, 0]
