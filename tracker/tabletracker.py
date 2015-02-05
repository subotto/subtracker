#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import copy
import cv2
import logging
import numpy


logger = logging.getLogger("table tracker")


class TableReferenceFrame:

    def __init__(self):
        self.image = cv2.imread("../data/ref.png")
        self.mask = cv2.imread("../data/ref_mask.png")
        self.lk_mask = cv2.imread("../data/ref_lk_mask.png")
        self.corners = numpy.float32(
            [[252, 70], [67, 65], [58, 183], [253, 186]])


class TableTracker:

    def __init__(self, table, reference):
        self.table = table
        self.reference = reference

        self.match_filter_ratio = 0.9
        self.match_ransac_threshold = 2.0

        self.detect_every_frames = 120
        self.near_transform_alpha = 0.25

        self.optical_flow_features = 100
        self.optical_flow_ransac_threshold = 1.0

        self.detector = cv2.BRISK()

        norm = cv2.NORM_HAMMING

        self.matcher = cv2.BFMatcher(norm)
        self.ref_kp, self.ref_des = self.detector.detectAndCompute(reference.image, reference.mask)

        self.base_transform = cv2.getPerspectiveTransform(table.ground.corners, reference.corners)

    def track_table(self, frames):
        transform = numpy.empty(frames.shape,)

        for i in numpy.ndindex(frames.shape):
            kp, des = self.detector.detectAndCompute(frames.image[i], None)
            matches = self.matcher.knnMatch(self.ref_des, trainDescriptors=des, k=2)
            p1, p2, kp_pairs = self.filter_matches(self.ref_kp, kp, matches)
            transform, mask = cv2.findHomography(p1, p2, cv2.RANSAC, self.match_ransac_threshold)

            frames.camera.transform[i] = transform.dot(self.base_transform)

    def filter_matches(self, kp1, kp2, matches, ratio=0.75):
        """Filter feature matching in order to sort out dubious matches.

        A query feature is considered dubious if it matches at least
        two training features and the distance ration between the
        first and second match is not at least the specified one. In
        case this happens, eliminate completely that query feature.

        It returns a tuple (p1, p2, kp_pairs); p1 is an array of the
        query features that did not result dubious. p2 is the array
        that contains the corresponding best training
        features. kp_pairs is just a zipping of the two lists.

        """
        mkp1, mkp2 = [], []
        for m in matches:
            if len(m) == 1 or (len(m) == 2 and m[0].distance < m[1].distance * ratio):
                m = m[0]
                mkp1.append(kp1[m.queryIdx])
                mkp2.append(kp2[m.trainIdx])
        p1 = numpy.float32([kp.pt for kp in mkp1])
        p2 = numpy.float32([kp.pt for kp in mkp2])
        kp_pairs = zip(mkp1, mkp2)
        return p1, p2, kp_pairs
