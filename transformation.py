#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import cv2
import logging
import numpy

logger = logging.getLogger(__name__)

RECTANGLE_POINTS = numpy.array([(-1.0, 1.0),
                                (-1.0, -1.0),
                                (1.0, 1.0),
                                (1.0, -1.0)],
                               dtype=numpy.float32)

def image_corners(width, height):
    """Returns the coordinates in pixel of the corners of an image
    with given size, taking into account that each pixel
    is a square of side 1 centered in the point identified
    by its coordinates.
    
    The result is a numpy array containing the points:
        (-0.5, -0.5)
        (-0.5, height - 0.5)
        (width - 0.5, -0.5)
        (width - 0.5, height - 0.5)
    """
    return numpy.array([(-0.5, -0.5),
                        (-0.5, height - 0.5),
                        (width - 0.5, -0.5),
                        (width - 0.5, height - 0.5)], dtype=numpy.float32);

def scale(x, y):
    """Returns a 3x3 projecective matrix that scales x and y axes with the given factors"""
    return numpy.float32([
        [x, 0, 0],
        [0, y, 0],
        [0, 0, 1]
    ])

def translate(dx, dy):
    """Returns a 3x3 projecective matrix that translates along the given vector"""
    return numpy.float32([
        [1, 0,dx],
        [0, 1,dy],
        [0, 0, 1]
    ])

def pixels_to_rectangle(width, height):
    """Returns a 3x3 projective matrix that maps an image with given size
    to a rectangle, in a way such that the boundary of the rectangle
    exactly coincides with the boundary of the outermost pixels.

    More specifically, the matrix will map:
        (-1/2, -1/2)                |--> (-1, +1)
        (-1/2, height - 1/2)        |--> (-1, -1)
        (width - 1/2, -1/2)         |--> (+1, +1)
        (width - 1/2, height - 1/2) |--> (+1, -1)

    """
    return cv2.getPerspectiveTransform(image_corners(width, height), RECTANGLE_POINTS)

def rectangle_to_pixels(width, height):
    """Returns the inverse of pixels_to_rectangle().

    """
    return cv2.getPerspectiveTransform(RECTANGLE_POINTS, image_corners(width, height))

def rectangle_to_region(corners):
    """Return a 3x3 projective matrix that maps a rectangle to the region
    described by four corners.

    More specifically, the matrix will map:
        (-1, -1) |--> first corner
        (+1, -1) |--> second corner
        (+1, +1) |--> third corner
        (-1, +1) |--> fourth corner

    """
    corners = numpy.vstack([corners[3],
                            corners[0],
                            corners[2],
                            corners[1]])
    #logger.info("%r", corners)
    #logger.info("%r", RECTANGLE_POINTS)
    return cv2.getPerspectiveTransform(RECTANGLE_POINTS, corners)
    
def region_to_rectangle(corners):
    """Returns the inverse of rectangle_to_region(corners)

    """
    corners = numpy.vstack([corners[3],
                            corners[0],
                            corners[2],
                            corners[1]])
    #logger.info("%r", corners)
    #logger.info("%r", RECTANGLE_POINTS)
    return cv2.getPerspectiveTransform(corners, RECTANGLE_POINTS)

def apply_projectivity(projectivity, point):
    """
    Returns the transformed point.
    The projectivity must be a 3x3 matrix. Both input and output points are 2-dimensional.
    """
    p = numpy.dot(projectivity, numpy.array([point[0], point[1], 1.0]))
    return p[:2] / p[2]

# FIXME: Some of following code is duplicated because of different conventions.
# We should always list corners in anti-clockwise order
# from bottom-left, which is (-w,+h) in image frame reference,
# and (-w,-h) in physical frame reference.
def get_image_corners(size):
    
    """Returns the coordinates in pixel of the corners of an image.
    
    size is a tuple containing image width and height.
    Returns a 4x2 array containing corner pixel coordinates
    in anti-clockwise order, from bottom left corner,
    taking into account that each pixel is a square of side 1
    centered in the point identified by its coordinates.
    """
    w, h = size
    return numpy.float32([
        ( -0.5, h-0.5),
        (w-0.5, h-0.5),
        (w-0.5,  -0.5),
        ( -0.5,  -0.5),
    ])


def get_rectangle_corners(size):
    """Returns the coordinates of the corners of a rectangle centered in 0, 0.
    
    size is a tuple containing rectangle width and height.
    Returns a 4x2 array containing corner coordinates
    in anti-clockwise order, from bottom left corner.
    """
    
    w, h = size
    x, y = w/2, h/2
    return numpy.float32([
        ( -x,  -y),
        (w-x,  -y),
        (w-x, h-y),
        ( -x, h-y),
    ])
    
def get_image_transform(surface_size, image_size):
    """Returns the transform that maps a rectangle
     of given size centered in 0, 0 to image pixels.
    """

    image_corners = get_image_corners(image_size)
    surface_corners = get_rectangle_corners(surface_size)
    return cv2.getPerspectiveTransform(surface_corners, image_corners)

def get_image_size(surface_size, resolution):
    """Computes the image size needed to map
    a rectangle of given size to pixels of an image,
    with the desired resolution in pixel per unit of length.
    
    resolution can be a number (for isotropic resolution)
    or a tuple containing two numbers: the resolution along X and Y axis respectively.
    """
    
    if not isinstance(resolution, tuple):
        resolution = resolution, resolution
    return tuple(int(numpy.ceil(surface_size[i]*resolution[i])) for i in (0,1))

def get_image_size_transform(surface_size, resolution):
    """Computes the image size and the transform to map a rectangle to an image.

    Returns a tuple of image size and transform,
    computed using get_image_size first and then get_image_transform.
    """

    image_size = get_image_size(surface_size, resolution)
    
    return image_size, get_image_transform(surface_size, image_size)



