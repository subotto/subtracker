#!/usr/bin/env python2
# coding=utf8


"""
Implementation of Gio-Giove algorithm to reconstruct ball trajectory.

USAGE
For each frame, create a list of Spot objects (relevant points of a frame, with coordinates and likelihood) and a Layer object with that list.
Then feed the frame to the SpotsTracker using SpotsTracker.push_back_and_get_info, which returns a list of couples (num_frame, estimated position).
"""

import collections
import itertools
from numpy import linalg


class SpotsTrackerSettings:
    
    def __init__(self):
        self.absent_ball_weight = 0.0    # Weight of a Spot with absent ball
        self.skip_badness = 0.0  # Badness added for every skipped frame
        
        self.disappearance_badness = 400.0
        self.appearance_badness = 400.0
        self.absence_badness = -10.0
        
        self.max_speed = 18.0
        self.max_unseen_distance = 0.3
        
        self.variance_parameter = 0.3
        
        self.dynamic_depth = 60


class Spot:
    """
    A relevant point of some frame.
    """
    
    def __init__(self, point, weight, frame_num=None, time=None):
        self.point = point
        self.weight = weight    # Likelihood
        
        self.frame_num = frame_num  # Frame number (possibly set by Layer)
        self.time = time    # Layer time (set by Layer)
        
        self.badness = 0.0  # Badness from starting spot to this spot (will be set by SpotsTracker.best_trajectory)
        self.prev_spot = None   # Previous spot in dynamic programming reconstruction (will be set by SpotsTracker.best_trajectory)
    
    def present(self):
        return self.point is not None


class Layer:
    """
    Contains all relevant information about a frame.
    """
    
    def __init__(self, spots, frame_num, time):
        self.spots = spots  # List of spots
        self.frame_num = frame_num
        self.time = time
        
        # Set frame_num and time for the spots
        for spot in spots:
            spot.frame_num = self.frame_num
            spot.time = self.time


class SpotsTracker:
    
    def __init__(self, settings=SpotsTrackerSettings()):
        self.timeline = collections.deque() # Deque of layers
        self.first_num = None   # frame_num of the first layer in the timeline
        
        self.fake_initial_spot = Spot(None, 0.0)    # Used in dynamic programming
        self.fake_final_spot = Spot(None, 0.0)       # Used in dynamic programming
        
        self.settings = settings
    
    
    def extended_spots(self):
        """
        Returns a generator of all the spots in the timeline, including fake_initial_spot and fake_final_spot.
        """
        yield self.fake_initial_spot
        for layer in self.timeline:
            for spot in layer.spots:
                yield spot
        yield self.fake_final_spot
    
    
    def push_back(self, layer):
        # Add the special Spot with absent ball to the layer
        layer.spots.append(Spot(None, self.settings.absent_ball_weight, frame_num=layer.frame_num, time=layer.time))
        
        self.timeline.append(layer)
        
        # Update self.first_num if the timeline was empty
        if len(self.timeline) == 1:
            self.first_num = self.timeline[0].frame_num
        
        # Update fake_final_spot.frame_num
        self.fake_final_spot.frame_num = layer.frame_num + 1
        
        # Update fake_initial_spot.frame_num if it is None (i.e. if this is the first insertion in the timeline)
        if self.fake_initial_spot.frame_num is None:
            self.fake_initial_spot.frame_num = self.timeline[0].frame_num - 1
        
        # Do dynamic programming for the new layer and for fake_final_spot
        for spot in itertools.chain(layer.spots, [self.fake_final_spot]):
            spot.badness = None
            spot.prev_spot = None
            
            # Find best trajectory until this spot, trying all possible previous spots
            for previous in self.extended_spots():
                if previous.frame_num < spot.frame_num:
                    # Try arc from previous to spot
                    badness = self.arc_badness(previous, spot)
                    if badness is not None:
                        badness += previous.badness
                        if spot.badness is None or badness < spot.badness:
                            # New badness is lower!
                            spot.badness = badness
                            spot.prev_spot = previous
        
    
    def pop_front(self):
        self.timeline.popleft()
        
        # Update self.first_num
        if len(self.timeline) == 0:
            self.first_num = None
        else:
            self.first_num = self.timeline[0].frame_num
    
    
    def arc_badness(self, start, end):
        """
        Returns the badness of the arc from Spot start to Spot end, or None if the arc does not exist.
        """
        skip = end.frame_num - start.frame_num - 1
        result = 0.0
        
        # Presence transitions
        if start.present() and not end.present() and end is not self.fake_final_spot:
            # Present to absent
            result += self.settings.disappearance_badness
        elif not start.present() and end.present() and start is not self.fake_initial_spot:
            # Absent to present
            result += self.settings.appearance_badness
        elif not start.present() and not end.present():
            # Absent to absent
            result += self.settings.absence_badness
            if skip > 0:
                return None
        
        # Node weight
        result -= end.weight
        
        # Skip badness
        result += float(skip) * self.settings.skip_badness
        
        # Locality check and gaussian likelihood
        if start.present() and end.present():
            time = end.time - start.time
            if time == 0.0:
                raise Exception("Different layers have the same timestamp")
            
            distance = linalg.norm(start.point - end.point)
            if distance > self.settings.max_speed * time:
                return None
            if distance > self.settings.max_unseen_distance:
                return None
            result += distance ** 2 / (time * self.settings.variance_parameter)
        
        return result
    
    
    def interpolate(self, first_spot, second_spot, time):
        a = first_spot.time
        b = second_spot.time
        k = (time - a) / (b - a)
        return (1 - k) * first_spot.point + k * second_spot.point
    
    
    def estimate_position(self):
        """
        Returns the estimated position of the ball for the first layer in the timeline, or None if the ball appears to be out of the field.
        """
        spot = self.fake_final_spot
        previous = spot.prev_spot
        
        """
        for spot in self.extended_spots():
            print spot.frame_num, spot.point, spot.weight, spot.badness,
            if spot.prev_spot is not None:
                print "Previous:", spot.prev_spot.frame_num, spot.prev_spot.point
        """
        
        while previous is not None and previous.frame_num > self.first_num:
            spot = previous
            previous = spot.prev_spot
        
        if previous is None or previous is self.fake_initial_spot or spot is self.fake_final_spot:
            # The ball appears to be out of the field
            return None
        
        elif previous.frame_num == self.first_num:
            # The estimate is done for the correct layer
            return previous.point
        
        else:
            # Interpolate between the two closest estimates
            return self.interpolate(previous, spot, self.timeline[0].time)
    
    
    def push_back_and_get_info(self, layer):
        self.push_back(layer)
        
        result = []
        
        while len(self.timeline) > self.settings.dynamic_depth:
            position = self.estimate_position()
            num_frame = self.first_num
            self.pop_front()
            result.append((num_frame, position))
        
        return result


