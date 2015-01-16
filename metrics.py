#!/usr/bin/python
# -*- coding: utf-8 -*-

# Distance values are in meters
FIELD_WIDTH = 1.135
FIELD_HEIGHT = 0.7

BALL_DIAMETER = 0.035

ROD_DIAMETER = 0.015
ROD_HEIGHT = 0.085
ROD_DISTANCE = 0.15
ROD_COLORS = ['red', 'blue']

# number of men, distance between men, displacent factor (multiply by ROD_DISTANCE)
ROD_CONFIGURATION = [(1,  0.0, -3.5),
                     (2, 0.24, -2.5),
                     (5, 0.12, -0.5),
                     (3, 0.21, +1.5)]
ROD_NUMBER = len(ROD_CONFIGURATION)

FOOSMAN_WIDTH = 0.03
FOOSMAN_HEAD_HEIGHT = 0.02
FOOSMAN_FEET_HEIGHT = 0.08

# Colors (BGR)
BALL_COLOR = (0.85, 0.85, 0.85)
FIELD_COLOR = (0.2, 0.7, 0.2)
ROD_COLOR = (0.6, 0.6, 0.6)
FOOSMEN_COLORS = [(0.10, 0.05, 0.65),
                  (0.50, 0.05, 0.25)]

# Private for viewer
CYLINDER_FACTOR = 10.0
