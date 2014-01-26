#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import os
import math
import threading
import urllib2
import time
import json
import Queue

import pygame
import pygame.locals

from OpenGL.GL import *
from OpenGL.GLU import *

from metrics import *

REQUEST_URL = 'http://uz.sns.it/24ore/tracking.json?last_timestamp=%(last_timestamp)f&convert_units=0'
REQUEST_TIMEOUT = 2.0
REQUEST_SLEEP = 0.5

def open_resource(x):
    return open(os.path.join(os.path.dirname(__file__), x))

def my_int(x):
    if x is None or x == '':
        return -1
    else:
        return int(x)

class RequestThread(threading.Thread):

    def __init__(self):
        threading.Thread.__init__(self)
        self.stop = False
        self.last_timestamp = 0.0
        self.daemon = True
        self.frames = Queue.Queue()

    def run(self):
        while not self.stop:
            try:
                response = urllib2.urlopen(REQUEST_URL % {'last_timestamp': self.last_timestamp}, timeout=REQUEST_TIMEOUT)
            except urllib2.URLError:
                print "Request failed or timed out"
                continue
            data = json.load(response)
            for frame in data['data']:
                self.frames.put(frame)
            if len(data['data']) > 0:
                self.last_timestamp = data['data'][-1]['timestamp']
            print self.last_timestamp
            time.sleep(REQUEST_SLEEP)

class ObjObject:

    def __init__(self):
        self.name = None
        self.vertices = []
        self.normals = []
        self.uv_vertices = []
        self.faces = []
        self.materials = []

    def draw(self):
        glBegin(GL_TRIANGLES)
        for face in self.faces:
            if face[0] == 'f':
                for vertex in face[1:]:
                    glNormal(*self.normals[vertex[2]-1])
                    glVertex(*self.vertices[vertex[0]-1])
            elif face[0] == 'usemtl':
                face[1].set()
            elif face[0] == 's':
                glEnd()
                if face[1]:
                    glShadeModel(GL_SMOOTH)
                else:
                    glShadeModel(GL_FLAT)
                glBegin(GL_TRIANGLES)
        glEnd()

class ObjMaterial:

    def __init__(self):
        self.name = None
        self.ambient = None
        self.diffuse = None
        self.specular = None
        self.transparency = None
        self.illumination_model = None
        self.diffuse_texture = None

    def set(self):
        glMaterial(GL_FRONT, GL_AMBIENT, self.ambient + (1.0,))
        glMaterial(GL_FRONT, GL_DIFFUSE, self.diffuse + (1.0,))
        glMaterial(GL_FRONT, GL_SPECULAR, self.specular)

def read_mtl(filename):

    materials = []
    cur_material = None

    with open_resource(filename) as fin:
        for line in fin:
            line = line.strip()
            if line == '' or line[0] == '#':
                continue
            tokens = line.split(' ')
            command = tokens[0]
            params = tokens[1:]

            if command == 'newmtl':
                assert len(params) == 1
                if cur_material is not None:
                    materials.append(cur_material)
                cur_material = ObjMaterial()
                cur_material.name = params[0]
            elif command == 'Ka':
                assert len(params) == 3
                cur_material.ambient = tuple([float(x) for x in params])
            elif command == 'Kd':
                assert len(params) == 3
                cur_material.diffuse = tuple([float(x) for x in params])
            elif command == 'Ks':
                assert len(params) == 3
                cur_material.specular = tuple([float(x) for x in params])
            elif command == 'd':
                assert len(params) == 1
                cur_material.illumination_model = float(params[0])
            elif command == 'map_Kd':
                assert len(params) == 1
                cur_material.diffuse_texture = params[0]

    if cur_material is not None:
        materials.append(cur_material)

    return materials

def read_obj(filename):

    objects = {}
    cur_object = None
    materials = {}

    offsets = [0, 0, 0]

    with open_resource(filename) as fin:
        for line in fin:
            line = line.strip()
            if line == '' or line[0] == '#':
                continue
            tokens = line.split(' ')
            command = tokens[0]
            params = tokens[1:]

            if command == 'o':
                assert len(params) == 1
                if cur_object is not None:
                    objects[cur_object.name] = cur_object
                    offsets[0] += len(cur_object.vertices)
                    offsets[1] += len(cur_object.uv_vertices)
                    offsets[2] += len(cur_object.normals)
                cur_object = ObjObject()
                cur_object.name = params[0]
            elif command == 'v':
                assert len(params) == 3
                cur_object.vertices.append(tuple([float(x) for x in params]))
            elif command == 'vt':
                assert len(params) == 2
                cur_object.uv_vertices.append(tuple([float(x) for x in params]))
            elif command == 'vn':
                assert len(params) == 3
                cur_object.normals.append(tuple([float(x) for x in params]))
            elif command == 'f':
                assert len(params) == 3
                face = []
                for param in params:
                    elems = [my_int(x) - offsets[i] for i, x in enumerate(param.split('/'))]
                    assert len(elems) == 3
                    face.append(tuple(elems))
                cur_object.faces.append(tuple(['f'] + face))
            elif command == 'usemtl':
                assert len(params) == 1
                cur_object.faces.append(('usemtl', materials[params[0]]))
            elif command == 's':
                assert len(params) == 1
                if params[0] == 'off':
                    cur_object.faces.append(('s', False))
                elif params[0] == '1':
                    cur_object.faces.append(('s', True))
                else:
                    assert False
            elif command == 'mtllib':
                assert len(params) == 1
                for material in read_mtl(*params):
                    materials[material.name] = material

    if cur_object is not None:
        objects[cur_object.name] = cur_object

    return objects

def resize(width, height):

    # Update pygame
    pygame.display.set_mode((width, height), pygame.OPENGL | pygame.DOUBLEBUF | pygame.HWSURFACE | pygame.RESIZABLE)

    # Use new viewport
    glViewport(0, 0, width, height)

    # Set up projection
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    gluPerspective(60.0, float(width)/float(height), 0.1, 500.0)

    # Set up trivial model view
    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()

def render(time):

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()
    #gluLookAt(0.8 * math.cos(time), 0.8 * math.sin(time), 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0)
    gluLookAt(0.0, -0.8, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0)

    glLight(GL_LIGHT0, GL_POSITION, (0.4, 0.6, 1.2))

MAX_LATENESS = 0.5

time_delta = None
current_frame = {
    'ball_x': 0.2,
    'ball_y': 0.1,
}

def update_timing(frames):
    global time_delta, current_frame

    now = time.time()

    if time_delta is not None:
        actual_time = now - time_delta
    empty_queue = False
    while True:
        try:
            frame = frames.get(block=False)
        except Queue.Empty:
            empty_queue = True
            break
        else:
            if time_delta is None:
                time_delta = now - frame['timestamp']
                actual_time = frame['timestamp']

            # If the frame is late, drop it
            if actual_time > frame['timestamp']:
                continue

            # If not, keep it and use it to draw next field
            else:
                current_frame = frame
                break

    # Synchronization check
    if time_delta is not None:

        # If there are no enqueued frames, do not let the time pass
        if empty_queue:
            time_delta += 1/fps

        # If we're too much behind, make a time jump
        else:
            lateness = current_frame['timestamp'] - actual_time
            if lateness > MAX_LATENESS:
                time_delta -= lateness

    print frames.qsize()

fps = 30.0

def main():

    # Spawn request thread
    thread = RequestThread()
    thread.start()

    # Initialize pygame stuff
    pygame.init()
    clock = pygame.time.Clock()
    resize(640, 480)
    pygame.display.set_caption('Subotto tracker viewer')

    # Initialize OpenGL stuff
    glEnable(GL_DEPTH_TEST)
    glShadeModel(GL_FLAT)
    glClearColor(0.0, 0.0, 0.0, 0.0)
    glEnable(GL_LIGHTING)
    glEnable(GL_LIGHT0)
    glLight(GL_LIGHT0, GL_DIFFUSE, (0.8, 0.8, 0.8))
    glLight(GL_LIGHT0, GL_AMBIENT, (0.1, 0.1, 0.1))

    objects = read_obj('omino.obj')

    frame = 0
    while True:

        # Receive and process events
        for event in pygame.event.get():

            if event.type == pygame.locals.QUIT:
                sys.exit(1)

            if event.type == pygame.locals.VIDEORESIZE:
                resize(event.w, event.h)

        update_timing(thread.frames)

        render(frame / fps)

        objects['Campo'].draw()

        for i in xrange(ROD_NUMBER):
            glPushMatrix()
            glTranslate((i - float(ROD_NUMBER-1) / 2.0) * ROD_DISTANCE, 0.0, ROD_HEIGHT)
            #glRotate(30.0, 0.0, 1.0, 0.0)
            glPushMatrix()
            glRotate(90.0, 1.0, 0.0, 0.0)
            glScale(ROD_DIAMETER * CYLINDER_FACTOR, ROD_DIAMETER * CYLINDER_FACTOR, FIELD_HEIGHT * CYLINDER_FACTOR)
            objects['Stecca'].draw()
            glPopMatrix()
            configuration = ROD_CONFIGURATION[i]
            for j in xrange(configuration[0]):
                glPushMatrix()
                glTranslate(0.0, (j - float(configuration[0]-1) / 2.0) * configuration[1], 0.0)
                if configuration[2] == 0:
                    objects['Omino_rosso'].draw()
                else:
                    objects['Omino_blu'].draw()
                glPopMatrix()
            glPopMatrix()

        glPushMatrix()
        glTranslate(current_frame['ball_x'],
                    current_frame['ball_y'],
                    0.0)
        objects['Pallina'].draw()
        glPopMatrix()

        print time_delta, current_frame['ball_x'], current_frame['ball_y']

        pygame.display.flip()
        clock.tick(fps)
        frame += 1

if __name__ == '__main__':
    main()
