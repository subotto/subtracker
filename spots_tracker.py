"""
Implementation of Gio-Giove algorithm to reconstruct ball trajectory.
"""

import collections
import itertools
import cv2



class Spot:
    """
    A relevant point of some frame.
    """
    
    def __init__(self, point, weight):
        self.point = point
        self.weight = weight
        self.frame_num = None  # Frame number (possibly set by Layer)
        self.time = None    # Layer time (set by Layer)
        
        self.badness = None # Badness from starting spot to this spot (will be set by SpotsTracker.best_trajectory)
        self.prev_spot = None   # Previous spot in dynamic programming reconstruction (will be set by SpotsTracker.best_trajectory)
    
    def present(self):
        return self.point is not None


class Layer:
    """
    Contains all relevant information about a frame.
    """
    
    def __init__(self, spots, frame_num, time):
        self.spots = spots  # List of spots
        
        # Add the special Spot with absent ball
        self.spots.append(Spot(None, SpotsTracker.ABSENT_BALL_WEIGHT))
        
        # Set frame_num and time for the spots
        for spot in spots:
            spot.frame_num = self.frame_num
            spot.time = self.time
        

class SpotsTracker:
    
    ABSENT_BALL_WEIGHT = 0.0    # Weight of a Spot with absent ball
    SKIP_BADNESS = 0.0  # Badness added for every skipped frame
    
    DISAPPEARANCE_BADNESS = 400.0
    APPEARANCE_BADNESS = 400.0
    ABSENCE_BADNESS = -10.0
    
    MAX_SPEED = 18.0
    MAX_UNSEEN_DISTANCE = 0.3
    
    VARIANCE_PARAMETER = 0.3
    
    
    def __init__(self):
        self.timeline = collections.deque() # Deque of layers
        self.first_num = None   # frame_num of the first layer in the timeline
        self.last_num = None    # frame_num of the last layer in the timeline
        
        self.fake_initial_spot = Spot(None, 0.0)    # Used in dynamic programming
        self.fake_final_spot = Spot(None, 0.0)       # Used in dynamic programming
    
    
    def push_back(self, layer):
        self.timeline.append(layer)
        
        # Update self.last_num
        self.last_num = layer.frame_num
        
        # Update self.first_num if the timeline was empty
        if len(self.timeline) == 1:
            self.first_num = self.timeline[0].frame_num
        
        # Update fake_final_spot
        self.fake_final_spot.frame_num += 1
        
        # Update fake_initial_spot.frame_num if it is None
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
    
    
    def extended_spots(self):
        """
        Returns a generator of all the spots in the timeline, including fake_initial_spot and fake_final_spot.
        """
        yield self.fake_initial_spot
        for layer in self.timeline:
            for spot in layer.spots:
                yield spot
        yield self.fake_final_spot
    
    
    def arc_badness(self, start, end):
        """
        Returns the badness of the arc from Spot start to Spot end, or None if the arc does not exist.
        """
        time = end.time - start.time
        skip = end.frame_num - start.frame_num - 1
        result = 0.0
        
        # Presence transitions
        if start.present() and not end.present():
            # Present to absent
            result += self.DISAPPEARANCE_BADNESS
        elif not start.present() and end.present():
            # Absent to present
            result += self.APPEARANCE_BADNESS
        elif not start.present() and not end.present():
            # Absent to absent
            result += self.ABSENCE_BADNESS
            if skip > 0:
                return None
        
        # Node weight
        result -= end.weight
        
        # Skip badness
        result += float(skip) * self.SKIP_BADNESS
        
        # Locality check and gaussian likelihood
        if start.present() and end.present():
            distance = cv2.norm(start.point - end.point)
            if distance > self.MAX_SPEED * time:
                return None
            if distance > self.MAX_UNSEEN_DISTANCE:
                return None
            result += distance ** 2 / (time * self.VARIANCE_PARAMETER)
        
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
            return self.interpolate(previous, spot, self.layers[0].time)
    
    
    def best_trajectory(self):
        """
        Returns a list of spots (at most one per layer) that is the best trajectory for the ball, and the badness of such trajectory.
        """
        
        for spot in self.extended_spots():
            if spot is self.fake_initial_spot:
                # Base step
                spot.badness = 0.0
                spot.prev_spot = None
            else:
                # Dynamic programming
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
        
        backward_trajectory = []
        spot = self.fake_final_spot
        while spot.prev_spot is not None and spot.prev_spot is not self.fake_initial_spot:
            spot = spot.prev_spot
            backward_trajectory.append(spot)
        
        trajectory = reversed(backward_trajectory)
        badness = self.fake_final_spot.badness
        
        return trajectory, badness


