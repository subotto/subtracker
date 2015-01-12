#!/usr/bin/python
# coding=utf8

from spotstracker import *

import cv2
import numpy

import unittest


class TestArcBadness(unittest.TestCase):
    
    def test_arc_badness(self):
        # Present to present, points too far away because of unseen distance
        
        tracker = SpotsTracker()
        tracker.settings.max_unseen_distance = 1.4
        tracker.settings.max_speed = 60.0
        
        start = Spot(point=numpy.array([1.0, 0.7]), weight=0.2, frame_num=5, time=0.0)
        end = Spot(point=numpy.array([0.2, -0.5]), weight=0.2, frame_num=10, time=0.1)
        # distance = 1.4422205101855958
        
        self.assertTrue(tracker.arc_badness(start, end) is None)
    
    def test_arc_badness2(self):
        # Present to present, points too far away because of max speed
        
        tracker = SpotsTracker()
        tracker.settings.max_unseen_distance = 3.0
        tracker.settings.max_speed = 10.0
        
        start = Spot(point=numpy.array([1.0, 0.7]), weight=0.2, frame_num=5, time=0.0)
        end = Spot(point=numpy.array([0.2, -0.5]), weight=0.2, frame_num=10, time=0.1)
        # distance = 1.4422205101855958
        
        self.assertTrue(tracker.arc_badness(start, end) is None)
    
    def test_arc_badness3(self):
        # Present to present, points close enough
        
        tracker = SpotsTracker()
        tracker.settings.max_unseen_distance = 3.0
        tracker.settings.max_speed = 60.0
        tracker.settings.skip_badness = 1.0
        tracker.settings.variance_parameter = 1.0
        
        start = Spot(point=numpy.array([1.0, 0.7]), weight=0.2, frame_num=5, time=0.0)
        end = Spot(point=numpy.array([0.2, -0.5]), weight=0.2, frame_num=10, time=0.1)
        # distance = 1.4422205101855958
        
        self.assertTrue( abs(tracker.arc_badness(start, end) - 24.6) < 1e-8 )
    
    def test_arc_badness4(self):
        # Present to absent
        
        tracker = SpotsTracker()
        tracker.settings.disappearance_badness = 7.0
        
        start = Spot(point=numpy.array([1.0, 0.7]), weight=0.2, frame_num=5, time=0.0)
        end = Spot(point=None, weight=0.0, frame_num=6, time=0.1)
        
        self.assertTrue( abs(tracker.arc_badness(start, end) - 7.0) < 1e-8 )
    
    def test_arc_badness5(self):
        # Absent to present
        
        tracker = SpotsTracker()
        tracker.settings.appearance_badness = 7.0
        
        start = Spot(point=None, weight=0.2, frame_num=5, time=0.0)
        end = Spot(point=numpy.array([1.0, 0.7]), weight=0.0, frame_num=6, time=0.1)
        
        self.assertTrue( abs(tracker.arc_badness(start, end) - 7.0) < 1e-8 )
    
    def test_arc_badness6(self):
        # Absent to absent
        
        tracker = SpotsTracker()
        tracker.settings.absence_badness = 7.0
        
        start = Spot(point=None, weight=0.2, frame_num=5, time=0.0)
        end = Spot(point=None, weight=0.0, frame_num=6, time=0.1)
        
        self.assertTrue( abs(tracker.arc_badness(start, end) - 7.0) < 1e-8 )
    
    def test_arc_badness7(self):
        # Absent to absent with skip > 0
        
        tracker = SpotsTracker()
        tracker.settings.absence_badness = 7.0
        
        start = Spot(point=None, weight=0.2, frame_num=5, time=0.0)
        end = Spot(point=None, weight=0.0, frame_num=7, time=0.1)
        
        self.assertTrue(tracker.arc_badness(start, end) is None)


class TestSpotsTracker(unittest.TestCase):
    
    def test_constant_path(self):
        settings = SpotsTrackerSettings()
        settings.dynamic_depth = 1
        settings.absence_badness = 0.0
        
        tracker = SpotsTracker(settings=settings)
        
        # Push first layer
        spots = [Spot(point=numpy.array([1.0, 0.7]), weight=0.3), Spot(point=numpy.array([0.2, -0.5]), weight=0.2)]
        layer = Layer(spots=spots, frame_num=5, time=0.0)
        
        num_frame, position = tracker.push_back_and_get_info(layer)
        self.assertTrue(num_frame is None)
        self.assertTrue(position is None)
        
        # Push second layer
        spots = [Spot(point=numpy.array([1.0, 0.7]), weight=0.3), Spot(point=numpy.array([0.2, -0.5]), weight=0.2)]
        layer = Layer(spots=spots, frame_num=6, time=0.1)
        
        num_frame, position = tracker.push_back_and_get_info(layer)
        self.assertEqual(num_frame, 5)
        self.assertTrue(numpy.allclose(position, numpy.array([1.0, 0.7])))



if __name__ == '__main__':
    unittest.main()



