#!/usr/bin/env python2
# -*- coding: utf-8 -*-

projective_transform = ("f8", (3, 3))


def image(size, channels, dtype="f4"):
    w, h = size
    c = channels
    return (dtype, (h, w, c))
