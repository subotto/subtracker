#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import threading
import time
import sys
import json
import os

sys.path.append(os.path.dirname(__file__))

from data import Session, Log, INTERESTING_FPS

TEST = True
TEST_BASEDIR = '../svgviewer'

BUFFER_TIME = 30.0
BUFFER_LEN = int(INTERESTING_FPS * BUFFER_TIME)
SLEEP_TIME = 0.5
MAX_QUERY_STRING_LEN = 512

class LogJSONEncoder(json.JSONEncoder):

    def default(self, obj):
        if isinstance(obj, Log):
            return obj.to_dict()
        else:
            return json.JSONEncoder.defaul(self, obj)

class Application:

    def __init__(self):
        self.buffer = []
        self.closing = False
        self.encoder = LogJSONEncoder(indent=2)

        self.thread = threading.Thread()
        self.thread.run = self.worker_run
        self.thread.daemon = True

        self.thread.start()

    def worker_run(self):
        session = Session()
        last_id = Log.get_last_id(session)
        if last_id is None:
            last_id = 0
        last_id -= BUFFER_LEN

        while not self.closing:
            new_data = session.query(Log).filter(Log.id > last_id).filter(Log.interesting == True).order_by(Log.id).all()
            if len(new_data) > 0:
                last_id = new_data[-1].id

            #print >> sys.stderr, "Read %d records" % (len(new_data))
            self.buffer += new_data
            self.buffer = self.buffer[-BUFFER_LEN:]
            session.expunge_all()
            session.rollback()
            time.sleep(SLEEP_TIME)

        session.rollback()
        session.close()

    def select_records(self, last_timestamp, convert_units):
        res = [x.clone() for x in self.buffer if x.timestamp > last_timestamp]
        if convert_units == 1:
            for x in res:
                x.convert_units()
        return res

    def error(self, environ, start_response):
        status = '400 Bad Request'
        response = '400 Bad Request'
        response_headers = [('Content-Type', 'text/plain; charset=utf-8'),
                            ('Content-Length', str(len(response)))]
        start_response(status, response_headers)
        return [response]

    def __call__(self, environ, start_response):
        # If TEST is enabled, we serve static files as a simple
        # webserver; needless to say, this piece of code has more or
        # less all the possible security bugs that one can invent, so
        # it is good to have it disabled before going live! ;-)
        if TEST and environ['PATH_INFO'] != '/24ore/tracking.json':
            try:
                with open(os.path.join(TEST_BASEDIR, environ['PATH_INFO'][1:])) as fin:
                    content = fin.read()
            except:
                content = '404 File not found'
                status = '404 File not found'
                #import traceback
                #traceback.print_exc()
            else:
                status = '200 OK'
            response_headers = [('Content-Length', str(len(content)))]
            start_response(status, response_headers)
            return [content]

        # Parse the request
        if 'QUERY_STRING' in environ and environ['QUERY_STRING'] is not None:
            query_string = environ['QUERY_STRING']
            if len(query_string) >= MAX_QUERY_STRING_LEN:
                return self.error(environ, start_response)
            try:
                request_tuples = [tuple(x.split('=', 1)) for x in environ['QUERY_STRING'].split('&')]
                request_params = dict([x for x in request_tuples if len(x) == 2])
                last_timestamp = float(request_params.get('last_timestamp', -1.0))
                convert_units = int(request_params.get('convert_units', 1))
            except Exception:
                #raise
                return self.error(environ, start_response)

        obj_response = {
            'data': self.select_records(last_timestamp, convert_units),
            'fps': INTERESTING_FPS,
            'buffer_len': BUFFER_LEN,
            'version': 1,
            }
        json_response = self.encoder.encode(obj_response).encode('utf-8')
        status = '200 OK'
        response_headers = [('Content-Type', 'application/json; charset=utf-8'),
                            ('Content-Length', str(len(json_response)))]
        start_response(status, response_headers)
        return [json_response]

application = Application()

if __name__ == '__main__':
    try:
        from wsgiref.simple_server import make_server
        httpd = make_server('', 8000, application)
        httpd.serve_forever()
    except KeyboardInterrupt:
        application.closing = True
