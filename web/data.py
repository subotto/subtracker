#!/usr/bin/python
# -*- coding: utf-8 -*-

from sqlalchemy import create_engine, Column, Integer, Float, DateTime
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, relationship, backref
from sqlalchemy.schema import Index
from sqlalchemy.orm.exc import NoResultFound
from sqlalchemy.orm import session as sessionlib

import datetime

with open('database_url') as fdata:
    database_url = fdata.readline().strip()
db = create_engine(database_url, echo=False)
Session = sessionmaker(db)
Base = declarative_base(db)

class Log(Base):
    __tablename__ = 'log'

    id = Column(Integer, primary_key=True)
    timestamp = Column(DateTime, nullable=False, index=True)

    ball_x = Column(Float, nullable=False)
    ball_y = Column(Float, nullable=False)
    rod_red_0_shift = Column(Float, nullable=False)
    rod_red_0_angle = Column(Float, nullable=False)
    rod_red_1_shift = Column(Float, nullable=False)
    rod_red_1_angle = Column(Float, nullable=False)
    rod_red_2_shift = Column(Float, nullable=False)
    rod_red_2_angle = Column(Float, nullable=False)
    rod_red_3_shift = Column(Float, nullable=False)
    rod_red_3_angle = Column(Float, nullable=False)

    rod_blue_0_shift = Column(Float, nullable=False)
    rod_blue_0_angle = Column(Float, nullable=False)
    rod_blue_1_shift = Column(Float, nullable=False)
    rod_blue_1_angle = Column(Float, nullable=False)
    rod_blue_2_shift = Column(Float, nullable=False)
    rod_blue_2_angle = Column(Float, nullable=False)
    rod_blue_3_shift = Column(Float, nullable=False)
    rod_blue_3_angle = Column(Float, nullable=False)

    @staticmethod
    def from_tuple(data):
        log = Log()
        log.timestamp = datetime.datetime.fromtimestamp(data[0])
        log.ball_x, log.ball_y, \
            log.rod_red_0_shift, log.rod_red_0_angle, \
            log.rod_red_1_shift, log.rod_red_1_angle, \
            log.rod_red_2_shift, log.rod_red_2_angle, \
            log.rod_red_3_shift, log.rod_red_3_angle, \
            log.rod_blue_0_shift, log.rod_blue_0_angle, \
            log.rod_blue_1_shift, log.rod_blue_1_angle, \
            log.rod_blue_2_shift, log.rod_blue_2_angle, \
            log.rod_blue_3_shift, log.rod_blue_3_angle \
            = data[1:]
        return log

    @staticmethod
    def get_last_id(session):
        last = session.query(Log).order_by(Log.timestamp.desc()).first()
        if last is None:
            return None
        else:
            return last.id

Base.metadata.create_all(db)
