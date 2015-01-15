#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import copy
import numpy
import cv2
import logging

logger = logging.getLogger("table tracker")


class TableDetectionSettings:

    def __init__(self):
        self.match_filter_ratio = 0.9
        self.match_ransac_threshold = 2.0

        self.ref_image_path = "data/ref.png"
        self.ref_mask_path = "data/ref_mask.png"
        self.ref_lk_mask_path = "data/ref_lk_mask.png"
        self.ref_table_points = numpy.float32(
            [[252, 72], [67, 67], [58, 183], [253, 186]])

        self.detector_name = "brisk"


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


def get_feature_detector(detector_name):
    if detector_name == 'sift':
        return cv2.SIFT(), cv2.NORM_L2
    elif detector_name == 'surf':
        return cv2.SURF(800), cv2.NORM_L2
    elif detector_name == 'usurf':
        return cv2.SURF(800, upright=True), cv2.NORM_L2
    elif detector_name == 'orb':
        return cv2.ORB(400), cv2.NORM_HAMMING
    elif detector_name == 'akaze':
        return cv2.AKAZE(), cv2.NORM_HAMMING
    elif detector_name == 'brisk':
        return cv2.BRISK(), cv2.NORM_HAMMING


class TableTracker:

    def __init__(self, prev_table_tracker, settings, controls):
        self.settings = settings
        self.controls = controls

        self.lk_track_points = None
        if prev_table_tracker is not None:
            self.prev = prev_table_tracker
            self.ref_kp = self.prev.ref_kp
            self.ref_des = self.prev.ref_des
            self.ref_image = self.prev.ref_image
            self.ref_mask = self.prev.ref_mask
            self.ref_lk_mask = self.prev.ref_lk_mask
            self.ref_table_points = self.prev.ref_table_points
            self.lk_assest_points = self.prev.lk_assest_points
        else:
            self.prev = None
            self.lk_assest_points = None
            self.load_ref()

    def load_ref(self):
        """Load reference images and compute features on them.

        """
        detection_settings = self.settings.detection_settings
        self.ref_image = cv2.imread(detection_settings.ref_image_path)
        self.ref_mask = cv2.imread(detection_settings.ref_mask_path)
        self.ref_lk_mask = cv2.imread(detection_settings.ref_lk_mask_path, cv2.IMREAD_GRAYSCALE)

        self.ref_table_points = detection_settings.ref_table_points

        detector, norm = get_feature_detector(detection_settings.detector_name)
        self.ref_kp, self.ref_des = detector.detectAndCompute(self.ref_image, self.ref_mask)

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

    def match_features(self):
        detection_settings = self.settings.detection_settings
        detector, norm = get_feature_detector(detection_settings.detector_name)

        kp, des = detector.detectAndCompute(self.frame, None)

        bf = cv2.BFMatcher(norm)

        matches = bf.knnMatch(self.ref_des, trainDescriptors=des, k=2)
        p1, p2, kp_pairs = self.filter_matches(self.ref_kp, kp, matches, ratio=detection_settings.match_filter_ratio)
        M, mask = cv2.findHomography(p1, p2, cv2.RANSAC, detection_settings.match_ransac_threshold)
        table_points = cv2.perspectiveTransform(detection_settings.ref_table_points.reshape(-1, 1, 2), M).reshape(-1, 2)

        return table_points

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

    def draw_frame_with_points(self, frame, points, title, points2=None):
        debug = frame.copy()
        for p in points:
            cv2.circle(debug, tuple(p[0]), 2, (255, 0, 0), -1)
        if points2 is not None:
            for p in points2:
                cv2.circle(debug, tuple(p[0]), 2, (0, 0, 255), -1)
        self.controls.show(title, debug / 256.0)

    def lk_track(self):
        p0 = numpy.array(self.prev.lk_track_points)
        begin_frame = self.prev.frame
        end_frame = self.frame

        begin_points, end_points = self.lk_flow(p0=p0, begin_frame=begin_frame, end_frame=end_frame)
        self.lk_track_points = end_points

        #self.draw_frame_with_points(begin_frame, p0, "track points before", begin_points)
        #self.draw_frame_with_points(end_frame, end_points, "track points after", end_points)

        begin_ref = numpy.array(begin_points)
        end_ref = numpy.array(end_points)
        return self.project_along(begin_ref, end_ref, self.prev.table_points.reshape(-1, 1, 2))

    def lk_assest(self):
        # If we have to few assest points, we recompute them (FIXME:
        # magic constants)
        if self.lk_assest_points is None or len(self.lk_assest_points) < 10:
            self.lk_assest_points = self.get_lk_points(self.ref_image, self.ref_table_points, self.lk_assest_points)

        # Warp reference frame to put in the same position as the
        # previously known corner points (FIXME: magic constants)
        detection_settings = self.settings.detection_settings
        H = cv2.getPerspectiveTransform(detection_settings.ref_table_points.reshape(-1, 1, 2), self.prev.table_points)
        warped_ref_image = cv2.warpPerspective(self.ref_image, H, (320, 240))
        self.controls.show("warped", warped_ref_image / 256.0)
        p0 = cv2.perspectiveTransform(self.lk_assest_points, H)

        begin_frame = warped_ref_image
        end_frame = self.frame

        begin_points, end_points = self.lk_flow(p0=p0, begin_frame=begin_frame, end_frame=end_frame)
        self.lk_assest_points = cv2.perspectiveTransform(numpy.array(begin_points), numpy.linalg.inv(H))

        #self.draw_frame_with_points(begin_frame, p0, "assest points before", begin_points)
        #self.draw_frame_with_points(end_frame, end_points, "assest points after", end_points)

        begin_ref = self.lk_assest_points
        end_ref = numpy.array(end_points)
        return self.project_along(begin_ref, end_ref, self.ref_table_points.reshape(-1, 1, 2))

    def lk_flow(self, p0, begin_frame, end_frame):
        """Run LK flow from begin_frame to end_frame along points in p0; also,
        selects the points that are considered to be more trustworthy
        for this flow. Returns such points both in the beginning end
        ending reference.

        """
        # FIXME: take this from settings
        lk_params = {"winSize": (15, 15),
                     "maxLevel": 2,
                     "criteria": (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 10, 0.03)}

        # Flow assest points forward; then flow them backward, so we
        # can check whether they match
        p1, st, err = cv2.calcOpticalFlowPyrLK(begin_frame, end_frame, p0, None, **lk_params)
        p0r, st, err = cv2.calcOpticalFlowPyrLK(end_frame, begin_frame, p1, None, **lk_params)

        # Retain points only if backflow was able to bring them in the
        # original place (except a certain error, which, BTW, is a
        # FIXME: magic constants)
        p0_good, p1_good = [], []
        for k in range(len(p0)):
            if numpy.linalg.norm(p0[k] - p0r[k]) < 1.0:
                p0_good.append(p0[k])
                p1_good.append(p1[k])

        return p0_good, p1_good

    def project_along(self, begin_ref, end_ref, points):
        """Project the points in points along the homography that is obtained
        as the RANSAC-best one that projects the swarm begin_ref to
        end_ref.

        """
        # FIXME: magic constants and unchecked status
        H1, status = cv2.findHomography(begin_ref, end_ref, cv2.RANSAC, 2.0)
        return cv2.perspectiveTransform(points, H1)

    def track_table(self, frame):
        """Return the four corners of the table as a tuple, in the following order:
          * red defence;
          * red attack;
          * blue defence;
          * blue attack.

        Each point is a numpy 2x1 matrix of floats. If nothing is
        found, then None is returned.

        """
        self.frame = frame
        self.table_points = None

        lk_assest_table_points = None
        lk_track_table_points = None

        # On first frame, we have nothing better than feature matching
        # to initialize everything.
        if self.prev is None:
            self.table_points = self.match_features()

        # Otherwise we try to evolve the previously known situation;
        # two different measurements are performed: the first
        # ("assest") implements an optical flow from a copy of the
        # reference frame warped according to the previous frame to
        # the current frame; the second ("track") implements an
        # optical flow from the previous frame to the current one.
        else:
            # FIXME: how can the following if fail?
            if self.prev.table_points is not None:
                lk_assest_table_points = self.lk_assest()
                if self.prev.lk_track_points is not None:
                    lk_track_table_points = self.lk_track()

        # If we have results from both "assest" and "track", then we
        # blend them together, estimating their variance (FIXME: magic
        # constants)
        if lk_track_table_points is not None or lk_track_table_points is not None:
            if lk_assest_table_points is None:
                self.table_points = lk_track_table_points
            elif lk_track_table_points is None:
                self.table_points = lk_assest_table_points
            else:
                trackVar = 0.3
                assestVar = 0.7
                gain = trackVar / (trackVar + assestVar)
                self.table_points = lk_track_table_points + gain * \
                                    (lk_assest_table_points - lk_track_table_points)

        # If we have to few trakc points, we recompute them (FIXME:
        # magic constants)
        if self.lk_track_points is None or len(self.lk_track_points) < 10:
            self.lk_track_points = self.get_lk_points(self.frame, self.table_points, self.lk_track_points)

        # Debug feedback
        debug_frame = frame.copy()
        cv2.polylines(
            debug_frame, [numpy.int32(self.table_points)], True, (0, 0, 255), 1)
        self.controls.show("table points", debug_frame / 256.0)

        # After the computation, forget the reference to previous
        # frame; this way the garbage collector is able to deallocate
        # old frames
        self.prev = None

        return self.table_points
