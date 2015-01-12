#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import cv2


def pixels_to_rectangle(width, height):
    """Returns a 3x3 projective matrix that maps an image with given size
    to a rectangle, in a way such that the boundary of the rectangle
    exactly coincides with the boundary of the outermost pixels.

    More specifically, the matrix will map:
        (-1/2, -1/2)                |--> (-1, 1)
        (-1/2, height + 1/2)        |--> (-1, -1)
        (width + 1/2, -1/2)         |--> (1, 1)
        (width + 1/2, height + 1/2) |--> (1, -1)

    """
    pixel_points = [(-0.5, -0.5),
                    (-0.5, height + 0.5),
                    (width + 0.5, -0.5),
                    (width + 0.5, height + 0.5)]
    rectangle_points = [(-1.0, 1.0),
                        (-1.0, -1.0),
                        (1.0, 1.0),
                        (1.0, -1.0)]
    return cv2.getPerspectiveTransform(pixel_points, rectangle_points)

def rectangle_to_pixels(width, height):
    """Returns the inverse of pixels_to_rectangle().

    """
    pixel_points = [(-0.5, -0.5),
                    (-0.5, height + 0.5),
                    (width + 0.5, -0.5),
                    (width + 0.5, height + 0.5)]
    rectangle_points = [(-1.0, 1.0),
                        (-1.0, -1.0),
                        (1.0, 1.0),
                        (1.0, -1.0)]
    return cv2.getPerspectiveTransform(rectangle_points, pixel_points)
