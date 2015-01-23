#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import sys
import time
import random
import math

FREQ = 120.0

mode = sys.argv[1]
if mode == 'random':
    field_num = int(sys.argv[2])

i = 0
start_time = time.time()
while True:

    now = time.time()

    if mode == 'random':
        print ",".join(["%.6f" % (now)] + [str(random.random()) for x in xrange(field_num)])

    elif mode == 'circle':
        omega = now
        ball_radius = 0.3
        shift_radius = 0.1
        angle_radius = math.pi / 2.0
        out = ["%.6f" % (now)]
        out += [str(x) for x in [ball_radius * math.cos(omega), ball_radius * math.sin(omega)]]
        for j in xrange(8):
            out += [str(x) for x in [shift_radius * math.sin(omega), angle_radius * math.cos(omega)]]
        print ",".join(out)

    elif mode == 'null':
        print "%.6f" % (now)

    sys.stdout.flush()
    time.sleep(1.0 / FREQ)
    i += 1
    if i % int(FREQ) == 0:
        print >> sys.stderr, "Did %d samples in %f seconds; average: %f samp/s" % (i, now - start_time, float(i) / (now - start_time))
        sys.stderr.flush()
