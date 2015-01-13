#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import copy
import numpy
import cv2

class TableDetectionSettings:

    def __init__(self):
        self.match_filter_ratio = 0.9
        self.match_ransac_threshold = 2.0
        
        self.ref_image_path="data/ref.png"
        self.ref_mask_path="data/ref_mask.png"
        self.ref_lk_mask_path="data/ref_lk_mask.png"
        self.ref_table_points = numpy.float32([ [252, 72],[67, 67],[58, 183],[253, 186] ])
    
        self.detector_name="brisk"

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
        
        self.optical_flow_points = None
        if prev_table_tracker is not None:
            self.prev=prev_table_tracker
            self.ref_kp = self.prev.ref_kp
            self.ref_des = self.prev.ref_des
            self.ref_image= self.prev.ref_image
            self.ref_mask= self.prev.ref_mask
            self.ref_lk_mask= self.prev.ref_lk_mask
        else:
            self.prev=None
            self.load_ref()
    
    def get_feature_detector(self, detector_name):
        detector=None
        norm=None
        if detector_name == 'sift':
            detector = cv2.SIFT()
            norm = cv2.NORM_L2
        elif detector_name == 'surf':
            detector = cv2.SURF(800)
            norm = cv2.NORM_L2
        elif detector_name == 'usurf':
            detector = cv2.SURF(800, upright=True)
            norm = cv2.NORM_L2
        elif detector_name == 'orb':
            detector = cv2.ORB(400)
            norm = cv2.NORM_HAMMING
        elif detector_name == 'akaze':
            detector = cv2.AKAZE()
            norm = cv2.NORM_HAMMING
        elif detector_name == 'brisk':
            detector = cv2.BRISK()
            norm = cv2.NORM_HAMMING
        return detector, norm
    
    def load_ref(self):
        detection_settings = self.settings.detection_settings 
        self.ref_image = cv2.imread(detection_settings.ref_image_path)
        self.ref_mask = cv2.imread(detection_settings.ref_mask_path)
        self.ref_lk_mask = cv2.imread(detection_settings.ref_lk_mask_path, cv2.IMREAD_GRAYSCALE)
        
        self.ref_table_points = detection_settings.ref_table_points
        
        detector, norm = self.get_feature_detector(detection_settings.detector_name)
        self.ref_kp, self.ref_des =  detector.detectAndCompute(self.ref_image,self.ref_mask)       
    
    def filter_matches(self, kp1, kp2, matches, ratio = 0.75):
        mkp1, mkp2 = [], []
        for m in matches:
            if len(m) == 1 or (len(m) == 2 and m[0].distance < m[1].distance * ratio):
                m = m[0]
                mkp1.append( kp1[m.queryIdx] )
                mkp2.append( kp2[m.trainIdx] )
        p1 = numpy.float32([kp.pt for kp in mkp1])
        p2 = numpy.float32([kp.pt for kp in mkp2])
        kp_pairs = zip(mkp1, mkp2)
        return p1, p2, kp_pairs
    
    def match_features(self):
        detection_settings = self.settings.detection_settings 
        detector, norm = self.get_feature_detector(detection_settings.detector_name)
        
        kp, des = detector.detectAndCompute(self.frame, None)

        bf = cv2.BFMatcher(norm)

        matches = bf.knnMatch(self.ref_des, trainDescriptors = des, k = 2)
        p1, p2, kp_pairs = self.filter_matches(self.ref_kp, kp, matches, ratio = detection_settings.match_filter_ratio)
        M, mask = cv2.findHomography(p1, p2, cv2.RANSAC, detection_settings.match_ransac_threshold)
        table_points = cv2.perspectiveTransform(detection_settings.ref_table_points.reshape(-1,1,2), M).reshape(-1,2) 
        return table_points
    
    def get_lk_points(self, img, table_points, old_lk_points):
        # FIXME use old_lk_points
        feature_params = dict( maxCorners = 1000,
                       qualityLevel = 0.01,
                       minDistance = 8,
                       blockSize = 19 )
        
        if table_points is None:
            return None
        
        img_gray=cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        #mask = numpy.zeros(img_gray.shape,dtype=numpy.uint8)
        #cv2.fillConvexPoly(mask, numpy.int32(table_points), 255)
        H=cv2.getPerspectiveTransform(self.ref_table_points, table_points)
        mask = cv2.warpPerspective(self.ref_lk_mask, H, (320,240))
        self.controls.show("lk mask", mask/256.0)
        return cv2.goodFeaturesToTrack(img_gray, mask=mask, **feature_params)
    
    def lk_track(self):
        lk_params = dict( winSize  = (15, 15),
                  maxLevel = 2,
                  criteria = (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 10, 0.03))
        p0 = numpy.array(self.prev.optical_flow_points)
        # Flow p0 forward
        p1, st, err = cv2.calcOpticalFlowPyrLK(self.prev.frame, self.frame, p0, None, **lk_params)
        # Flow p1 backward to check if it coincides with p0
        p0r, st, err = cv2.calcOpticalFlowPyrLK(self.frame, self.prev.frame, p1, None, **lk_params)
        p0_good, p1_good = [], []
        for k in range(len(p0)):
            if numpy.linalg.norm(p0[k] - p0r[k]) < 1:
                p0_good.append(p0[k])
                p1_good.append(p1[k])
        self.optical_flow_points=p1_good
        # DEBUG
        debug = self.frame.copy()
        for p in self.optical_flow_points:
            cv2.circle(debug, tuple(p[0]), 2, (255,0,0), -1)
        self.controls.show("lk points", debug/256.0)
        # Compute homography using good points
        H, status = cv2.findHomography(numpy.array(p0_good), numpy.array(p1_good),cv2.RANSAC, 2.0)
        table_points = cv2.perspectiveTransform(self.prev.table_points.reshape(-1,1,2), H)
        return table_points
    
    def lk_assest(self):
        pass
    
    def track_table(self, frame):
        """Return the four corners of the table as a tuple, in the following order:
          * red defence;
          * red attack;
          * blue defence;
          * blue attack.

        Each point is a numpy 2x1 matrix of floats. If nothing is
        found, then None is returned.

        """
        self.frame=frame
        self.table_points = None
        # ---- COMPUTE NEW TALBLE POINTS -----
        if self.prev is None:
            self.table_points = self.match_features()
        lk_assest_table_points = None
        if self.prev is not None and self.prev.table_points is not None:
            lk_assest_table_points = self.lk_assest()
        lk_track_table_points = None
        if self.prev is not None and self.prev.table_points is not None and self.prev.optical_flow_points is not None:
            lk_track_table_points = self.lk_track()
        # ---- UPDATE ----
        trackVar = 0.5
        assestVar = 0.5
        gain = trackVar / (trackVar + assestVar);
        lk_assest_table_points = lk_track_table_points if lk_assest_table_points is None else lk_assest_table_points
        lk_track_table_points = lk_assest_table_points if lk_track_table_points is None else lk_track_table_points
        if lk_track_table_points is not None:
            self.table_points = lk_track_table_points + gain * (lk_assest_table_points - lk_track_table_points)
        # ---- RECOMPUTE LK POINTS ----
        if self.optical_flow_points is None or len(self.optical_flow_points) < 10:
            self.optical_flow_points = self.get_lk_points(self.frame, self.table_points, self.optical_flow_points)
        # ---- DEBUG ----
        debug_frame = frame.copy()
        cv2.polylines(debug_frame,[numpy.int32(self.table_points)],True,(0,255,0),3)
        # test controls are working
        #coeff = self.controls.trackbar("coeff", initial_value = 0.02, min_value = 0.01, max_value = 1.0).value
        self.controls.show("table points", debug_frame/256.0)

        return self.table_points

        # After the computation, forget the reference to previous
        # frame; this way the garbage collector is able to deallocate
        # old frames
        self.prev = None
