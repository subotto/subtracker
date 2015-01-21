#!/usr/bin/python
# -*- coding:utf-8 -*-

import sys
import time

from data import Session, Log, INTERESTING_FPS

COMMIT_FREQ = 2.0

def my_float(x):
    if x == '':
        return None
    else:
        return float(x)

if __name__ == '__main__':
    session = Session()

    last_commit = time.time()
    last_interesting = 0.0
    try:
        while True:
            line = sys.stdin.readline()
            if line == '':
                break
            data = [my_float(x) for x in line.strip().split(",")]
            timestamp = float(data[0])
            log = Log.from_tuple(data)
            if timestamp - last_interesting >= 1.0 / INTERESTING_FPS:
                log.interesting = True
                last_interesting = timestamp
            session.add(log)
            session.flush()
            now = time.time()
            if now - last_commit >= COMMIT_FREQ:
                last_commit = now
                # After the implicit flush, all the objects in the
                # session are weakly references by the session, so
                # they're immediately pruned; i.e., this shouldn't
                # have memory leaks
                session.commit()

    except KeyboardInterrupt:
        pass

    finally:
        session.commit()
