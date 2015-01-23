#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import sys
import time

from data import Session, Log

if __name__ == '__main__':
    session = Session()
    for log in session.query(Log):
        session.delete(log)
    session.commit()
