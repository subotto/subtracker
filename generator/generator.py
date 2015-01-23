#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import sys
import math
import random

import cairo
import cv
import cv2
import numpy
import numpy.lib

from metrics import *

SEP = 0.05

fps = 120
length = None
size = (320, 240)

def draw_cross(ctx):
    ctx.new_path()
    ctx.move_to(-1.0, -1.0)
    ctx.line_to(1.0, 1.0)
    ctx.move_to(1.0, -1.0)
    ctx.line_to(-1.0, 1.0)
    ctx.stroke()

def draw_square(ctx):
    ctx.new_path()
    ctx.move_to(-1.0, -1.0)
    ctx.line_to(1.0, -1.0)
    ctx.line_to(1.0, 1.0)
    ctx.line_to(-1.0, 1.0)
    ctx.close_path()
    ctx.stroke()

def fill_square(ctx):
    ctx.new_path()
    ctx.move_to(-1.0, -1.0)
    ctx.line_to(1.0, -1.0)
    ctx.line_to(1.0, 1.0)
    ctx.line_to(-1.0, 1.0)
    ctx.close_path()
    ctx.fill()

def draw_circle(ctx):
    ctx.new_path()
    ctx.arc(0.0, 0.0, 1.0, 0.0, 2*math.pi)
    ctx.close_path()
    ctx.stroke()

def draw_oplus(ctx):
    ctx.new_path()
    ctx.arc(0.0, 0.0, 1.0, 0.0, 2*math.pi)
    ctx.close_path()
    ctx.move_to(-1.0, 0.0)
    ctx.line_to(1.0, 0.0)
    ctx.move_to(0.0, -1.0)
    ctx.line_to(0.0, 1.0)
    ctx.stroke()

def perturb(val, sigma, max_):
    val += random.gauss(0.0, sigma)
    val = min(max_, max(-max_, val))
    return val

class FrameContext:
    trans_x = 0.0
    trans_y = 0.0
    angle = 0.0

    # At each step data are perturbed adding a Gaussian term with zero
    # mean and specified standard deviation
    trans_x_sigma = 0.005
    trans_y_sigma = 0.005
    angle_sigma = 0.005

    # At each step data are clamped in order to not to deviate from
    # zero for more than specified values
    trans_x_max = 0.1
    trans_y_max = 0.1
    angle_max = 0.3

    def perturb_all(self):
        self.trans_x = perturb(self.trans_x, self.trans_x_sigma, self.trans_x_max)
        self.trans_y = perturb(self.trans_y, self.trans_y_sigma, self.trans_y_max)
        self.angle = perturb(self.angle, self.angle_sigma, self.angle_max)

    def apply_to_cairo(self, ctx):
        ctx.rotate(self.angle)
        ctx.translate(self.trans_x, self.trans_y)

class SubottoStatus:
    ball_pos = (0.0, 0.0)  # (x, y)
    rods_pos = [(0.0, 0.0) for _ in xrange(ROD_NUMBER)]  # (shift, angle)
    free_space = [FIELD_HEIGHT - (num - 1) * sep - FOOSMAN_WIDTH
                  for num, sep, _ in ROD_CONFIGURATION]
    rods_shift_sigma = 0.005
    rods_angle_sigma = 0.05

    def update(self, time):
        radius = 0.25
        freq = 0.3
        angle = time * freq * 2 * math.pi

        # First frame is skipped so we can easily take the reference frame
        if time == 0.0:
            self.ball_pos = None
            return

        # The balls move on a circle
        self.ball_pos = (radius * math.cos(angle), radius * math.sin(angle))

        # Perturbe randomly rods position
        for i in xrange(ROD_NUMBER):
            shift, angle = self.rods_pos[i]
            shift = perturb(shift, self.rods_shift_sigma, self.free_space[i]/2)
            angle = perturb(angle, self.rods_angle_sigma, math.pi/2)
            self.rods_pos[i] = shift, angle

    def draw_to_cairo(self, ctx):
        # Fill the field
        ctx.rectangle(-FIELD_WIDTH/2, -FIELD_HEIGHT/2, FIELD_WIDTH, FIELD_HEIGHT)
        ctx.set_source_rgb(*FIELD_COLOR)
        ctx.fill()

        # Fill the ball
        if self.ball_pos is not None:
            ctx.arc(self.ball_pos[0], self.ball_pos[1], BALL_DIAMETER/2, 0.0, 2*math.pi)
            ctx.set_source_rgb(1.0, 1.0, 1.0)
            ctx.fill()

        # Fill the rods
        if self.rods_pos is not None:
            for i, (num, sep, col) in enumerate(ROD_CONFIGURATION):
                shift, angle = self.rods_pos[i]
                ctx.save()
                ctx.translate(ROD_DISTANCE * (i - (ROD_NUMBER - 1) / 2.0), 0.0)
                ctx.set_source_rgb(*ROD_COLOR)
                ctx.rectangle(-ROD_DIAMETER/2, -FIELD_HEIGHT/2, ROD_DIAMETER, FIELD_HEIGHT)
                ctx.fill()
                ctx.set_source_rgb(*FOOSMEN_COLORS[col])
                ctx.translate(0.0, shift)
                # Hack: if angle is zero, set it to a small value
                # to make matrix invertible
                if angle == 0.0:
                    angle = 0.00001
                for j in xrange(num):
                    ctx.save()
                    ctx.translate(0.0, sep * (j - (num - 1) / 2.0))
                    ctx.rectangle(-ROD_DIAMETER/2, -FOOSMAN_WIDTH/2, ROD_DIAMETER, FOOSMAN_WIDTH)
                    ctx.fill()
                    ctx.scale(math.sin(angle), 1.0)
                    ctx.rectangle(-FOOSMAN_HEAD_HEIGHT, -FOOSMAN_WIDTH/2, FOOSMAN_HEAD_HEIGHT + FOOSMAN_FEET_HEIGHT, FOOSMAN_WIDTH)
                    ctx.fill()
                    ctx.restore()
                ctx.restore()

        # Stroke the field
        ctx.rectangle(-FIELD_WIDTH/2, -FIELD_HEIGHT/2, FIELD_WIDTH, FIELD_HEIGHT)
        ctx.set_source_rgb(0.0, 0.0, 0.0)
        ctx.stroke()

def draw_frame(ctx, frame_ctx, status, size, time):
    # Paint background
    ctx.save()
    ctx.identity_matrix()
    ctx.rectangle(0, 0, size[0], size[1])
    ctx.set_source_rgb(0.3, 0.1, 0.1)
    ctx.fill()
    ctx.restore()

    # Randomize field position
    ctx.save()
    frame_ctx.perturb_all()
    frame_ctx.apply_to_cairo(ctx)

    # Evolve model
    status.update(time)

    # Draw field from model
    status.draw_to_cairo(ctx)

    # Draw tracking features
    ctx.save()
    ctx.set_line_width(FIELD_HEIGHT/10)
    for i, j, mark, bg_color in [(1, 1, draw_oplus, (1.0, 0.0, 0.0)),
                                 (1, -1, draw_cross, (0.0, 1.0, 0.0)),
                                 (-1, 1, draw_circle, (0.0, 0.0, 1.0)),
                                 (-1, -1, draw_square, (1.0, 1.0, 0.0))]:
        ctx.save()
        ctx.translate(i * (FIELD_WIDTH/2 + FIELD_HEIGHT/6 + SEP), j * (FIELD_HEIGHT * (0.5 - 1.0/6)))
        ctx.scale(FIELD_HEIGHT/6, FIELD_HEIGHT/6)
        ctx.set_source_rgb(*bg_color)
        fill_square(ctx)
        ctx.set_source_rgb(0.0, 0.0, 0.0)
        mark(ctx)
        ctx.restore()
    ctx.restore()

    # Restore from randomized position
    ctx.restore()

def setup_pygame():
    # Initialize PyGame
    global pygame
    import pygame
    import pygame.locals
    pygame.init()
    pygame.display.set_caption('generator.py')

    pygame_surf = pygame.display.set_mode(size, pygame.DOUBLEBUF, 32)
    return pygame.surfarray.pixels2d(pygame_surf)

def setup_cairo(data=None):
    # Inizialize cairo surface and context
    if data is None:
        surf = cairo.ImageSurface(cairo.FORMAT_RGB24, size[0], size[1])
    else:
        surf = cairo.ImageSurface.create_for_data(data, cairo.FORMAT_RGB24, size[0], size[1])
    ctx = cairo.Context(surf)

    # Set up a reasonable coordinate system
    versor_len = 0.7 * min(size)
    reflect = cairo.Matrix(1.0, 0.0, 0.0, -1.0, 0.0, 0.0)
    ctx.transform(reflect)
    ctx.translate(size[0]/2, -size[1]/2)
    ctx.scale(versor_len, versor_len)

    ctx.set_line_width(1.5 / versor_len)
    ctx.set_line_join(cairo.LINE_JOIN_ROUND)
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)

    return surf, ctx

def save_frames(surf, ctx):
    if length is not None:
        frames = int(fps * length)
    else:
        frames = None

    # Create video writer
    vw = cv2.VideoWriter('test.avi', cv.FOURCC('D', 'I', 'V', 'X'), fps, size)
    #vw = cv2.VideoWriter('test.avi', cv.FOURCC('M', 'J', 'P', 'G'), fps, size)

    frame = 0
    frame_ctx = FrameContext()
    status = SubottoStatus()
    while frames is None or frame < frames:
        print "Writing frame %d..." % (frame),

        # Draw the frame
        time = float(frame) / fps
        draw_frame(ctx, frame_ctx, status, size, time)
        surf.flush()

        # Write it to a file
        if frame == 0:
            with open('frames/frame_%05d.png' % (frame), 'w') as fout:
                surf.write_to_png(fout)

        # Write it to the video
        strides = (surf.get_stride(), 4, 1)
        shape = (size[1], size[0], 3)
        im = numpy.frombuffer(surf.get_data(), dtype=numpy.uint8)
        #print len(im), shape, strides, surf.get_stride()
        #im.shape = shape
        #im.strides = strides
        im = numpy.lib.stride_tricks.as_strided(im, shape, strides)
        #print im
        #im = numpy.empty((size[1], size[0], 3), numpy.uint8)
        vw.write(im)

        print "done!"
        frame += 1

def show_pygame(surf, ctx):
    if length is not None:
        frames = int(fps * length)
    else:
        frames = None

    frame = 0
    frame_ctx = FrameContext()
    status = SubottoStatus()
    fpsClock = pygame.time.Clock()
    while frames is None or frame < frames:
        # Process events
        for event in pygame.event.get():
            if event.type == pygame.locals.QUIT:
                pygame.quit()
                sys.exit()

            elif event.type == pygame.locals.KEYDOWN:
                if event.key == pygame.locals.K_ESCAPE:
                    pygame.event.post(pygame.event.Event(pygame.locals.QUIT))

        print "Writing frame %d..." % (frame),

        # Draw the frame
        time = float(frame) / fps
        draw_frame(ctx, frame_ctx, status, size, time)
        surf.flush()

        print "done!"
        frame += 1

        # Finish
        pygame.display.flip()
        fpsClock.tick(fps)

def main():
    with_pygame = 'show' in sys.argv
    data = None

    if with_pygame:
        data = setup_pygame()

    surf, ctx = setup_cairo(data)

    if with_pygame:
        show_pygame(surf, ctx)
    else:
        save_frames(surf, ctx)

if __name__ == '__main__':
    main()
1
