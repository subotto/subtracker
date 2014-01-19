#!/usr/bin/python
# -*- coding: utf-8 -*-

from sqlalchemy import create_engine, Column, Integer, Float
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, relationship, backref
from sqlalchemy.schema import Index
from sqlalchemy.orm.exc import NoResultFound
from sqlalchemy.orm import session as sessionlib

with open('database_url') as fdata:
    database_url = fdata.readline().strip()

# Set isolation level to READ UNCOMMITTED for better responsiveness
db = create_engine(database_url, echo=False, isolation_level="READ UNCOMMITTED")
Session = sessionmaker(db)
Base = declarative_base(db)

class Log(Base):
    __tablename__ = 'log'

    id = Column(Integer, primary_key=True)
    #timestamp = Column(DateTime, nullable=False)
    timestamp = Column(Float, nullable=False)

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

    def to_tuple(self):
        return (self.timestamp, self.ball_x, self.ball_y,
                self.rod_red_0_shift, self.rod_red_0_angle,
                self.rod_red_1_shift, self.rod_red_1_angle,
                self.rod_red_2_shift, self.rod_red_2_angle,
                self.rod_red_3_shift, self.rod_red_3_angle,
                self.rod_blue_0_shift, self.rod_blue_0_angle,
                self.rod_blue_1_shift, self.rod_blue_1_angle,
                self.rod_blue_2_shift, self.rod_blue_2_angle,
                self.rod_blue_3_shift, self.rod_blue_3_angle)

    @staticmethod
    def from_tuple(data):
        log = Log()
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
    def get_last_id(session):
        last = session.query(Log).order_by(Log.id.desc()).first()
        if last is None:
            return None
        else:
            return last.id

Base.metadata.create_all(db)
