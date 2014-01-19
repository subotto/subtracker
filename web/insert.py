#!/usr/bin/python
# -*- coding:utf-8 -*-

import sys

from data import Session, Log

COMMIT_FREQ = 120

if __name__ == '__main__':
    session = Session()

    log_num = 0
    try:
        for line in sys.stdin:
            data = [float(x) for x in line.strip().split(",")]
            log = Log.from_tuple(data)
            session.add(log)
            log_num += 1
            if log_num % COMMIT_FREQ == 0:
                session.commit()
    except KeyboardInterrupt:
        pass
