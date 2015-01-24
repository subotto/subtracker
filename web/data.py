#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import math
import os

from sqlalchemy import create_engine, Column, Integer, Float, Boolean
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, relationship, backref
from sqlalchemy.schema import Index
from sqlalchemy.orm.exc import NoResultFound
from sqlalchemy.orm import session as sessionlib

INTERESTING_FPS = 30.0

with open(os.path.join(os.path.dirname(__file__), 'database_url')) as fdata:
    database_url = fdata.readline().strip()

# Set isolation level to READ UNCOMMITTED for better responsiveness
# (but it doesn't work anyway...)
db = create_engine(database_url, echo=False, isolation_level="READ UNCOMMITTED")
Session = sessionmaker(db)
Base = declarative_base(db)

class Log(Base):
    __tablename__ = 'log'

    id = Column(Integer, primary_key=True)
    #timestamp = Column(DateTime, nullable=False)
    timestamp = Column(Float, nullable=False)

    ball_x = Column(Float, nullable=True)
    ball_y = Column(Float, nullable=True)
    rod_red_0_shift = Column(Float, nullable=True)
    rod_red_0_angle = Column(Float, nullable=True)
    rod_red_1_shift = Column(Float, nullable=True)
    rod_red_1_angle = Column(Float, nullable=True)
    rod_red_2_shift = Column(Float, nullable=True)
    rod_red_2_angle = Column(Float, nullable=True)
    rod_red_3_shift = Column(Float, nullable=True)
    rod_red_3_angle = Column(Float, nullable=True)

    rod_blue_0_shift = Column(Float, nullable=True)
    rod_blue_0_angle = Column(Float, nullable=True)
    rod_blue_1_shift = Column(Float, nullable=True)
    rod_blue_1_angle = Column(Float, nullable=True)
    rod_blue_2_shift = Column(Float, nullable=True)
    rod_blue_2_angle = Column(Float, nullable=True)
    rod_blue_3_shift = Column(Float, nullable=True)
    rod_blue_3_angle = Column(Float, nullable=True)

    interesting = Column(Boolean, nullable=False, default=False)

    TUPLE_LEN = 1 + 2 + 2 * 8

    INTERESTING_FIELDS = (
        'timestamp', 'ball_x', 'ball_y',
        'rod_red_0_shift', 'rod_red_0_angle',
        'rod_red_1_shift', 'rod_red_1_angle',
        'rod_red_2_shift', 'rod_red_2_angle',
        'rod_red_3_shift', 'rod_red_3_angle',
        'rod_blue_0_shift', 'rod_blue_0_angle',
        'rod_blue_1_shift', 'rod_blue_1_angle',
        'rod_blue_2_shift', 'rod_blue_2_angle',
        'rod_blue_3_shift', 'rod_blue_3_angle')

    # Size in meters
    DIM_X = 115e-2
    DIM_Y = 70e-2
    BALL_RADIUS = 2e-2

    def convert_units(self):
        for f in Log.INTERESTING_FIELDS:
            if self.__dict__[f] is None:
                continue
            if f == 'ball_x':
                self.__dict__[f] = 2.0 * self.__dict__[f] / Log.DIM_X
            if f == 'ball_y':
                self.__dict__[f] = 2.0 * self.__dict__[f] / Log.DIM_Y
            if f.startswith('rod') and f.endswith('shift'):
                self.__dict__[f] = 2.0 * self.__dict__[f] / Log.DIM_Y
            if f.startswith('rod') and f.endswith('angle'):
                self.__dict__[f] = self.__dict__[f] - math.pi/2

    def to_tuple(self):
        return tuple([self.__dict__[f] for f in Log.INTERESTING_FIELDS])

    def to_dict(self):
        return dict([(f, self.__dict__[f]) for f in Log.INTERESTING_FIELDS])

    def clone(self):
        return Log.from_tuple(self.to_tuple())

    @staticmethod
    def from_tuple(data):
        log = Log()
        if len(data) < Log.TUPLE_LEN:
            data = list(data) + [None] * (Log.TUPLE_LEN - len(data))
        log.timestamp, log.ball_x, log.ball_y, \
            log.rod_red_0_shift, log.rod_red_0_angle, \
            log.rod_red_1_shift, log.rod_red_1_angle, \
            log.rod_red_2_shift, log.rod_red_2_angle, \
            log.rod_red_3_shift, log.rod_red_3_angle, \
            log.rod_blue_0_shift, log.rod_blue_0_angle, \
            log.rod_blue_1_shift, log.rod_blue_1_angle, \
            log.rod_blue_2_shift, log.rod_blue_2_angle, \
            log.rod_blue_3_shift, log.rod_blue_3_angle \
            = data
        return log

    @staticmethod
    def get_last_id(session, simulate_time=None):
        query = session.query(Log).order_by(Log.id.desc())
        if simulate_time is not None:
            query = query.filter(Log.timestamp <= simulate_time)
        last = query.first()
        if last is None:
            return None
        else:
            return last.id

Base.metadata.create_all(db)
