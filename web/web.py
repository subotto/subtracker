#!/usr/bin/python
# -*- coding: utf-8 -*-

import threading
import time

from data import Session, Log

BUFFER_LEN = 240
WAIT_TIME = 1.0

class Application:

    def __init__(self):
        self.buffer = []

        self.thread = threading.Thread()
        self.thread.run = self.worker_run

        self.thread.start()

    def worker_run(self):
        session = Session()
        last_id = Log.get_last_id(session)
        time.wait(WAIT_TIME)

    def __call__(self, environ, start_reponse):
        pass

application = Application()
