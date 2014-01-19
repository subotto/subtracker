#!/usr/bin/python
# -*- coding: utf-8 -*-

import threading
import time
import json

from data import Session, Log

BUFFER_LEN = 240
SLEEP_TIME = 0.1

class LogJSONEncoder(json.JSONEncoder):

    def default(self, obj):
        if isinstance(obj, Log):
            return obj.to_tuple()
        else:
            return json.JSONEncoder.defaul(self, obj)

class Application:

    def __init__(self):
        self.buffer = []
        self.closing = False
        self.encoder = LogJSONEncoder()

        self.thread = threading.Thread()
        self.thread.run = self.worker_run
        self.thread.damon = True

        self.thread.start()

    def worker_run(self):
        session = Session()
        last_id = Log.get_last_id(session)
        if last_id is None:
            last_id = 0
        last_id -= BUFFER_LEN
        while not self.closing:
            self.buffer += session.query(Log).filter(Log.id > last_id).order_by(Log.id).all()
            if len(self.buffer) > 0:
                last_id = self.buffer[-1].id
            self.buffer = self.buffer[-BUFFER_LEN:]
            time.sleep(SLEEP_TIME)
        session.rollback()
        session.close()

    def select_records(self, record_num, interval):
        return self.buffer[-record_num:]

    def error(self, environ, start_response):
        status = '400 Bad Request'
        response = '400 Bad Request'
        response_headers = [('Content-Type', 'text/plain; charset=utf-8'),
                            ('Content-Length', str(len(response)))]
        start_response(status, response_headers)
        return [response]

    def __call__(self, environ, start_response):
        # Parse the request
        if 'QUERY_STRING' in environ and environ['QUERY_STRING'] is not None:
            query_string = environ['QUERY_STRING']
            if len(query_string) >= 512:
                return self.error(environ, start_response)
            try:
                request_tuples = [tuple(x.split('=', 1)) for x in environ['QUERY_STRING'].split('&')]
                request_params = dict([x for x in request_tuples if len(x) == 2])
                record_num = int(request_params.get('record_num', 24))
                interval = float(request_params.get('interval', 1.0 / 24))
            except Exception:
                #raise
                return self.error(environ, start_response)

        json_response = self.encoder.encode(self.select_records(record_num, interval)).encode('utf-8')
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
