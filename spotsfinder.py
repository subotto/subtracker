#!/usr/bin/python
# coding=utf8


"""
Find spots (relevant points) of a frame.

USAGE
Call SpotsFinder.find_spots giving in input a numpy 2D-array with the ballness of the frame.
The output is a list of Spot objects (see spotstracker.py) sorted by ballness (descending).
"""

import numpy
from spotstracker import Spot


class SpotsFinderSettings:
    
    def __init__(self):
        self.num_spots = 10 # Maximum number of spots to return
        self.threshold = 0.0    # Minimum log-likelihood of relevant spots


class SpotsFinder:
    
    def __init__(self, settings=SpotsFinderSettings()):
        self.settings = settings
    
    def find_spots(self, ballness, frame_num=None, time=None):
        """
        Given the ballness of all the pixel of a frame, returns the best pixels.
        """
        # FIXME: find spots in some more clever way
        
        local_maxima = []
        
        for i, row in enumerate(ballness):
            for j, b in enumerate(row):
                if b >= self.settings.threshold:
                    
                    local_max = True
                    for i1,j1 in [(i+1,j), (i-1,j), (i,j+1), (i,j-1)]:
                        if i1 > 0 and j1 > 0 and i1 < ballness.shape[0] and j1 < ballness.shape[1]:
                            if ballness[i1][j1] > b:
                                local_max = False
                                break
                    
                    if local_max:
                        local_maxima.append((b,i,j))
        
        local_maxima.sort(reverse=True)
        best_pixels = local_maxima[:self.settings.num_spots]
        
        # FIXME: do the right change of reference (pixels -> meters)
        best_spots = [Spot(point=numpy.array((float(i)/320, float(j)/240)), weight=b, frame_num=frame_num, time=time) for (b,i,j) in best_pixels]
        
        return best_spots



