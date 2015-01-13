#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import copy

class TableDetectionSettings:

    def __init__(self):
        self.reference_features_per_level = 300
        self.reference_features_levels = 3

        self.frame_features_per_level = 500
        self.frame_features_levels = 3

        self.features_knn = 2

        self.coarse_ransac_threshold = 20.0
        self.coarse_ransac_outliers_ratio = 0.75

        self.optical_flow_features_per_level = 200
        self.optical_flow_features_levels = 3

        self.optical_flow_ransac_threshold = 1.0


class TableFollowingSettings:

    def __init__(self):
        self.optical_flow_features = 100
        self.optical_flow_ransac_threshold = 1.0


class TableTrackingSettings:

    def __init__(self):
        self.detect_every_frames = 120
        self.near_transform_alpha = 0.25

        self.detection_settings = TableDetectionSettings()
        self.following_settings = TableFollowingSettings()


class TableTracker:

    def __init__(self, prev_table_tracker, settings, controls):
        self.settings = settings
        self.controls = controls

        # TODO: take data from prev_table_tracker or initialize it

    def track_table(self, frame):
        # TODO

        # test controls are working
        coeff = self.controls.trackbar("coeff", initial_value = 0.02, min_value = 0.01, max_value = 1.0).value
        self.controls.show("raw frame", frame * coeff)
