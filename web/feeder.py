#!/usr/bin/python
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
        radius = 0.3
        print ",".join(["%.6f" % (now)] + [str(x) for x in [radius * math.cos(omega), radius * math.sin(omega)]])

    elif mode == 'null':
        print "%.6f" % (now)

    sys.stdout.flush()
    time.sleep(1.0 / FREQ)
    i += 1
    if i % int(FREQ) == 0:
        now = time.time()
        print >> sys.stderr, "Did %d samples in %f seconds; average: %f samp/s" % (i, now - start_time, float(i) / (now - start_time))
        sys.stderr.flush()
