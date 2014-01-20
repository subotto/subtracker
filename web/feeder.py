#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import time
import random

FREQ = 24.0

FIELDS_NUM = 18
FIELDS_NUM = 2

i = 0
start_time = time.time()
while True:
    print ",".join(["%.6f" % (time.time())] + [str(random.random()) for x in xrange(FIELDS_NUM)])
    sys.stdout.flush()
    time.sleep(1.0 / FREQ)
    i += 1
    if i % int(FREQ) == 0:
        now = time.time()
        print >> sys.stderr, "Did %d samples in %f seconds; average: %f samp/s" % (i, now - start_time, float(i) / (now - start_time))
        sys.stderr.flush()
