#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import math

import cairo
import cv
import cv2
import numpy
import numpy.lib

FIELD_WIDTH = 1.135
FIELD_HEIGHT = 0.7

def draw_frame(ctx, size, time):
    # Everything white
    ctx.save()
    ctx.identity_matrix()
    ctx.rectangle(0, 0, size[0], size[1])
    ctx.set_source_rgb(255, 255, 255)
    ctx.fill()
    ctx.restore()

    ctx.rectangle(-FIELD_WIDTH/2, -FIELD_HEIGHT/2, FIELD_WIDTH, FIELD_HEIGHT)
    ctx.set_source_rgb(0.2, 7.0, 0.2)
    ctx.fill()

    ctx.rectangle(-FIELD_WIDTH/2, -FIELD_HEIGHT/2, FIELD_WIDTH, FIELD_HEIGHT)
    ctx.set_source_rgb(0.0, 0.0, 0.0)
    ctx.stroke()

    radius = 0.25
    freq = 0.5
    angle = time * freq * 2 * math.pi
    ctx.arc(radius * math.cos(angle), radius * math.sin(angle), 0.035/2, 0.0, 2*math.pi)
    ctx.set_source_rgb(1.0, 1.0, 1.0)
    ctx.fill()

def save_frames():
    fps = 120
    length = None
    size = (320, 240)

    if length is not None:
        frames = int(fps * length)
    else:
        frames = None
    cairo_surf = cairo.ImageSurface(
        cairo.FORMAT_RGB24,
        size[0],
        size[1])

    # Inizialize cairo context
    ctx = cairo.Context(cairo_surf)

    # Set up a reasonable coordinate system
    versor_len = 1.0 * min(size)
    reflect = cairo.Matrix(1.0, 0.0, 0.0, -1.0, 0.0, 0.0)
    ctx.transform(reflect)
    ctx.translate(size[0]/2, -size[1]/2)
    ctx.scale(versor_len, versor_len)

    ctx.set_line_width(1.5 / versor_len)
    ctx.set_line_join(cairo.LINE_JOIN_ROUND)
    ctx.set_line_cap(cairo.LINE_CAP_ROUND)

    # Create video writer
    vw = cv2.VideoWriter('test.avi', cv.FOURCC('D', 'I', 'V', 'X'), fps, size)
    #vw = cv2.VideoWriter('test.avi', cv.FOURCC('M', 'J', 'P', 'G'), fps, size)

    frame = 0
    while frames is None or frame < frames:
        print "Writing frame %d..." % (frame),

        # Draw the frame
        time = float(frame) / fps
        draw_frame(ctx, size, time)
        cairo_surf.flush()

        # Write it to a file
        #with open('frames/frame_%05d.png' % (frame), 'w') as fout:
        #    cairo_surf.write_to_png(fout)

        # Write it to the video
        strides = (cairo_surf.get_stride(), 4, 1)
        shape = (size[1], size[0], 3)
        im = numpy.frombuffer(cairo_surf.get_data(), dtype=numpy.uint8)
        #print len(im), shape, strides, cairo_surf.get_stride()
        #im.shape = shape
        #im.strides = strides
        im = numpy.lib.stride_tricks.as_strided(im, shape, strides)
        #print im
        #im = numpy.empty((size[1], size[0], 3), numpy.uint8)
        vw.write(im)

        print "done!"
        frame += 1

def main():
    save_frames()

if __name__ == '__main__':
    main()
