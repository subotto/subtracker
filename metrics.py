#!/usr/bin/env python2
# -*- coding: utf-8 -*-

from collections import namedtuple

TEAMS = 2

Rod = namedtuple("Rod", ("location_x", "men"))
Foosman = namedtuple("Foosman", ("location_y"))

class Metrics:
    
    def __init__(self):
        # all length measures are in meters
    
        self.ground_size = (1.135, 0.7)
        self.ball_diameter = 0.035

        self.rod_diameter = 0.015
        self.rod_location_z = 0.085

        self.rod_distance = d = 0.15

        self.max_men_num = 5
        self.rods = [
            Rod(-3.5 * d, [Foosman(0.0)]),
            Rod(-2.5 * d, [Foosman(-0.12), Foosman(+0.12)]),
            Rod(-0.5 * d, [Foosman(-0.24), Foosman(-0.12), Foosman(0.0), Foosman(+0.12), Foosman(+0.24)]),
            Rod(+1.5 * d, [Foosman(-0.21), Foosman(0.0), Foosman(+0.21)]),
        ]
        
        self.foosman_width = 0.03
        self.foosman_head_height = 0.02
        self.foosman_feet_height = 0.08

        # Colors (BGR)
        self.ball_color = (0.85, 0.85, 0.85)
        self.field_color = (0.2, 0.7, 0.2)
        self.rod_color = (0.6, 0.6, 0.6)
        self.foosmen_color = [
            (0.10, 0.05, 0.65),
            (0.50, 0.05, 0.25),
        ]

