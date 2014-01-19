#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import time
import random

i = 0
start_time = time.time()
while True:
    print ",".join(["%.6f" % (time.time())] + [str(random.random()) for x in xrange(18)])
    time.sleep(1.0 / 120)
    i += 1
    if i % 120 == 0:
        now = time.time()
        print >> sys.stderr, "Did %d samples in %f seconds; average: %f samp/s" % (i, now - start_time, float(i) / (now - start_time))
