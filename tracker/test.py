#!/usr/bin/env python2
# coding=utf8


from cv2 import VideoCapture

import cv2
import sys

from analysis import GameAnalyzer, GameAnalysis
from camera import Camera
from table import Table


images = {}


def show(name, image, contrast=1.0, brightness=1.0):
    def on_change(*args):
        c = cv2.getTrackbarPos("contrast", name)
        b = cv2.getTrackbarPos("brightness", name)
        image = images[name]
        cv2.imshow(name, image * 10 ** ((c - 1000) / 100.0) + (b - 1000) / 100.0)

    if cv2.getTrackbarPos("contrast", name) == -1:
        cv2.namedWindow(name, cv2.WINDOW_NORMAL)
        cv2.createTrackbar("contrast", name, int(1000 * contrast), 2000, on_change)
        cv2.createTrackbar("brightness", name, int(1000 * brightness), 2000, on_change)
    images[name] = image
    on_change()


def move_to_block(index, cap):
    property_pos_frames = 1  # OpenCV magic number
    frames = 1200
    cap.set(property_pos_frames, frames * index)


def run():
    table = Table()
    camera = Camera(frame_rate=120, frame_size=(320, 240), channels=3)
    game_analyzer = GameAnalyzer(table, camera)

    cap = VideoCapture("../data/video.mp4")

    analysis = GameAnalysis(game_analyzer)

    f = 0
    block_index = 10

    table = Table()
    team = 0
    rod = 0
    man = 0
    play = False

    while True:
        move_to_block(block_index, cap)

        camera.capture(cap, analysis)

        game_analyzer.analyze(analysis)

        print "Block: ", block_index

        show("background.color_mean", analysis.background.color_mean)

        while True:
            max_f = game_analyzer.frames_in_flight - 1

            f = max(0, min(f, max_f))
            team %= len(table.teams)
            rod %= len(table.rods)
            man %= len(table.rods[rod].foosmen)

            if not play or f % 30 == 0:
                print "Frame: %5d   team: %5s   rod: %4s %10s   foosman: %1d" % (
                    f, table.teams[team].name, table.rods[rod].team.name, table.rods[rod].type.name, man
                )

            show("image", analysis.frames.image[f])
            show("image_f", analysis.frames.image_f[f])
            show("rectification", analysis.frames.rectification[f])
            show("frame.background.ll", analysis.frames.background.color_analysis.ll[f])
            show("frame.team_foosmen.ll", analysis.frames.team_foosmen.color_analysis.ll[f, team])
            show("frame.ball.color_analysis.ll", analysis.frames.ball.color_analysis.ll[f])

            show("frame.team_foosmen.present_ll", analysis.frames.team_foosmen.present_ll_c[f, team])
            show("frame.team_foosmen.absent_ll", analysis.frames.team_foosmen.absent_ll_c[f, team])
            show("frame.team_foosmen.present_llr", analysis.frames.team_foosmen.present_llr[f, team])
            show("frame.team_foosmen.location_llr", analysis.frames.team_foosmen.location_llr[f, team])

            show("frame.ball.present_llr", analysis.frames.ball.present_llr[f])
            show("frame.ball.location_llr", analysis.frames.ball.location_llr[f])

            show("frame.rod.foosman.shift_llr", analysis.frames.rod.field(rod).foosman.shift_llr[f, man])

            if play:
                key_code = cv2.waitKey(10)
                f = (f + 1) % game_analyzer.frames_in_flight
            else:
                key_code = cv2.waitKey()

            key = chr(key_code & 255)

            if key == ".":
                play = False
                f += 1
            if key == ",":
                play = False
                f -= 1

            if key == "q":
                sys.exit(0)

            if key == "t":
                team += 1

            if key == "e":
                rod += 1
                man = 0
            if key == "r":
                rod -= 1
                man = 0

            if key == "m":
                man += 1
            if key == "n":
                man -= 1

            if key == "j":
                f = 0
            if key == " ":
                play = not play

            if key == "<":
                block_index -= 1
                f = game_analyzer.frames_in_flight - 1
                break
            if key == ">":
                block_index += 1
                f = 0
                break

if __name__ == "__main__":
    run()
