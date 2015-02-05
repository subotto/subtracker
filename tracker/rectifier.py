#!/usr/bin/env python2
# coding=utf8


import cv2
import numpy

from transformation import get_image_size_transform


class Rectifier:

    def __init__(self, table, margin, camera, resolution):
        self.table = table
        self.margin = margin

        self.channels = camera.channels

        self.surface_size = tuple(s + margin for s in table.ground.size)
        self.resolution = resolution

        self.image_size, self.transform = get_image_size_transform(self.surface_size, self.resolution)

    def rectify(self, frames):
        frames.image_f[...] = frames.image / 255.0

        transform = numpy.einsum("...ij,...jk", self.transform, numpy.linalg.inv(frames.table.camera_transform))

        for i in numpy.ndindex(frames.shape):
            frames.rectification[i] = cv2.warpPerspective(frames.image_f[i], transform[i], self.image_size)

