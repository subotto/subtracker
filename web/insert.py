#!/usr/bin/python
# -*- coding:utf-8 -*-

import sys
import time

from data import Session, Log

COMMIT_FREQ = 2.0

def my_float(x):
    if x == '':
        return None
    else:
        return float(x)

if __name__ == '__main__':
    session = Session()

    last_commit = time.time()
    try:
        while True:
            line = sys.stdin.readline()
            if line == '':
                break
            data = [my_float(x) for x in line.strip().split(",")]
            log = Log.from_tuple(data)
            session.add(log)
            session.flush()
            now = time.time()
            if now - last_commit >= COMMIT_FREQ:
                last_commit = now
                session.commit()

    except KeyboardInterrupt:
        pass

    finally:
        session.commit()
