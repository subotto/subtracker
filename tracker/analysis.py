#!/usr/bin/env python2
# coding=utf8


import numpy

from background import BackgroundAnalyzer
from ball import BallAnalyzer
from commontypes import projective_transform
from foosmen import TeamFoosmenAnalyzer, RodAnalyzer
from occlusion import OcclusionAnalyzer
from rectifier import Rectifier
from tabletracker import TableTracker, TableReferenceFrame


class GameAnalyzer:

    def __init__(self, table, camera):
        self.table = table
        self.camera = camera
        self.frames_in_flight = 120

        self.table_tracker = TableTracker(table, camera, TableReferenceFrame())
        self.rectifier = Rectifier(table, margin=0.10, camera=camera, resolution=120)

        self.background_analyzer = BackgroundAnalyzer(table, self.rectifier)
        self.team_foosmen_analyzer = TeamFoosmenAnalyzer(table, self.rectifier)
        self.ball_analyzer = BallAnalyzer(table, self.rectifier)

        self.rod_analyzer = [RodAnalyzer(table, rod, self.rectifier) for rod in table.rods]

    def analyze(self, data):
        self.table_tracker.locate_table(data)
        self.table_tracker.settle_table(data)
        self.table_tracker.compute_camera_transform(data)

        self.rectifier.rectify(data.frames)

        self.background_analyzer.estimate_color(data)
        self.background_analyzer.analyze_color(data)

        for i, team in enumerate(self.table.teams):
            self.team_foosmen_analyzer.initialize_model(team, data)
            self.team_foosmen_analyzer.analyze_color(team, data)
            self.team_foosmen_analyzer.compute_visible_llr(team, data)
            self.team_foosmen_analyzer.compute_location_llr(team, data)

        for i, rod in enumerate(self.table.rods):
            analyzer = self.rod_analyzer[i]

            for j, foosman in enumerate(rod.foosmen):
                foosman_analyzer = analyzer.foosman_analyzer[j]
                foosman_analyzer.compute_shift_llr(data)

            analyzer.compute_shift_llr(data)

        self.ball_analyzer.analyze_color(data)
        self.ball_analyzer.compute_visible_llr(data)
        self.ball_analyzer.compute_location_llr(data)


class GameAnalysis:

    def __init__(self, analyzer):
        n = analyzer.frames_in_flight

        fw, fh = analyzer.camera.frame_size
        c = analyzer.camera.channels

        rw, rh = analyzer.rectifier.image_size

        self.frames = numpy.recarray(n, [
            ("index", "u4"),
            ("timestamp", "f8"),
            ("image", "u1", (fh, fw, c)),
            ("image_f", "f4", (fh, fw, c)),
            ("table", analyzer.table_tracker.dtype),
            ("rectification", "f4", (rh, rw, c)),
            ("background", analyzer.background_analyzer.dtype),
            ("team_foosmen", analyzer.team_foosmen_analyzer.dtype, len(analyzer.table.teams)),
            ("ball", analyzer.ball_analyzer.dtype),
            ("rod", [
                ("%s_%s" % (rod.type.name, rod.team.name), analyzer.rod_analyzer[i].dtype) for i, rod in enumerate(analyzer.table.rods)
            ])
        ])

        self.background = numpy.recarray((), [
            ("color_mean", "f4", (rh, rw, c)),
            ("variance", "f8", (c, c)),
            ("q_estimation", "f4", (rh, rw))
        ])

        self.team_foosmen = numpy.recarray(len(analyzer.table.teams), [
            ("color_mean", "f4", (rh, rw, c)),
            ("variance", "f8", (c, c)),
        ])
