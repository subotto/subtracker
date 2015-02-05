#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import copy
import cv2
import logging
import numpy
from commontypes import projective_transform


logger = logging.getLogger("table tracker")


class TableReferenceFrame:

    def __init__(self):
        self.image = cv2.imread("../data/ref.png")
        self.mask = cv2.imread("../data/ref_mask.png", cv2.IMREAD_GRAYSCALE)
        self.corners = numpy.float32(
            [[252, 70], [67, 65], [58, 183], [253, 186]])


class TableTracker:

    def __init__(self, table, camera, reference):
        self.table = table
        self.camera = camera
        self.reference = reference

        # FIXME: some of the following are not used
        self.match_filter_ratio = 0.9
        self.match_ransac_threshold = 2.0

        self.optical_flow_ransac_threshold = 1.0

        self.detector = cv2.BRISK()

        norm = cv2.NORM_HAMMING

        self.matcher = cv2.BFMatcher(norm)

        # features for matching
        self.ref_kp, self.ref_des = self.detector.detectAndCompute(reference.image, mask=reference.mask)

        self.of_params = {
            "maxCorners": 1000,
            "qualityLevel": 0.01,
            "minDistance": 8,
            "blockSize": 19,
        }

        # features for optical flow
        self.ref_of_kp = numpy.concatenate([
            cv2.goodFeaturesToTrack(reference.image[..., c], mask=reference.mask, **self.of_params) for c in xrange(camera.channels)
        ])

        self.base_transform = cv2.getPerspectiveTransform(table.ground.corners, reference.corners)

        w, h = camera.frame_size

        # reference transform maps reference frame to current frame
        # camera transform maps table coordinates to current frame
        self.dtype = [
            ("coarse_reference_transform", projective_transform),
            ("settled_reference_transform", projective_transform),
            ("camera_transform", projective_transform),
            ("pixel_density", "f4", (h, w)),
        ]

    def get_lk_points(self, img, table_points, old_lk_points):
        # FIXME use old_lk_points (for what? What is supposed to be
        # there? What this method is supposed to compute?)
        feature_params = {"maxCorners": 1000,
                          "qualityLevel": 0.01,
                          "minDistance": 8,
                          "blockSize": 19}

        if table_points is None:
            return None

        img_gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        # mask = numpy.zeros(img_gray.shape,dtype=numpy.uint8)
        # cv2.fillConvexPoly(mask, numpy.int32(table_points), 255)
        H = cv2.getPerspectiveTransform(self.ref_table_points, table_points)
        mask = cv2.warpPerspective(self.ref_lk_mask, H, (320, 240))
        return cv2.goodFeaturesToTrack(img_gray, mask=mask, **feature_params)

    def locate_table(self, data):
        for i in numpy.ndindex(data.frames.shape):
            kp, des = self.detector.detectAndCompute(data.frames.image[i], None)
            matches = self.matcher.knnMatch(self.ref_des, trainDescriptors=des, k=2)
            p1, p2, kp_pairs = self.filter_matches(self.ref_kp, kp, matches)

            data.frames.table.coarse_reference_transform[i], mask = cv2.findHomography(p1, p2, cv2.RANSAC, self.match_ransac_threshold)

    def settle_table(self, data):
        for i in numpy.ndindex(data.frames.shape):
            H = data.frames.table.coarse_reference_transform[i]
            warped_ref_image = cv2.warpPerspective(self.reference.image, H, self.camera.frame_size)
            warped_ref_points = cv2.perspectiveTransform(self.ref_of_kp, H)

            points, status, error = cv2.calcOpticalFlowPyrLK(warped_ref_image, data.frames.image[i], warped_ref_points)

            good = status != 0
            correction_transform, status = cv2.findHomography(warped_ref_points[good], points[good], cv2.RANSAC)

            data.frames.table.settled_reference_transform[i] = correction_transform.dot(H)

    def compute_camera_transform(self, data):
        table = data.frames.table
        table.camera_transform[...] = numpy.einsum("...ij,...jk", table.settled_reference_transform, self.base_transform)

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
