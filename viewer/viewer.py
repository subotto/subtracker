#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import os

import pygame
import pygame.locals

from OpenGL.GL import *
from OpenGL.GLU import *

class ObjObject:

    def __init__(self):
        self.name = None
        self.vertices = []
        self.normals = []
        self.uv_vertices = []
        self.faces = []
        self.material = None

    def draw(self):
        glBegin(GL_TRIANGLES)
        for face in self.faces:
            for vertex in face:
                glNormal(*self.normals[vertex[2]-1])
                glVertex(*self.vertices[vertex[0]-1])
        glEnd()

def read_obj(filename):

    objects = []
    cur_object = None

    with open(filename) as fin:
        for line in fin:
            line = line.strip()
            if line[0] == '#':
                continue
            tokens = line.split(' ')
            command = tokens[0]
            params = tokens[1:]

            if command == 'o':
                if cur_object is not None:
                    objects.append(cur_objects)
                cur_object = ObjObject()
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
                    elems = [int(x) for x in param.split('/')]
                    assert len(elems) == 3
                    face.append(tuple(elems))
                cur_object.faces.append(tuple(face))

    if cur_object is not None:
        objects.append(cur_object)

    return objects

def resize(width, height):

    # Use new viewport
    glViewport(0, 0, width, height)

    # Set up projection
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    gluPerspective(35.0, float(width)/float(height), 0.1, 100.0)

    # Set up trivial model view
    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()

def render(time):

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()
    #gluLookAt(2.66, -3.0, 1.85, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0)
    gluLookAt(0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 1.0, 0.0)

    glScale(0.3, 0.3, 0.3)
    glBegin(GL_QUADS)
    glNormal(0.0, 0.0, 1.0)
    glVertex(-1.0, -1.0, 0.0)
    glVertex(-1.0, 1.0, 0.0)
    glVertex(1.0, 1.0, 0.0)
    glVertex(1.0, -1.0, 0.0)
    glEnd()

FPS = 30.0

def main():

    # Initialize pygame stuff
    pygame.init()
    clock = pygame.time.Clock()
    surface = pygame.display.set_mode((640, 480), pygame.OPENGL | pygame.DOUBLEBUF | pygame.HWSURFACE | pygame.RESIZABLE)
    resize(640, 480)
    pygame.display.set_caption('Subotto tracker viewer')

    # Initialize OpenGL stuff
    glEnable(GL_DEPTH_TEST)
    glShadeModel(GL_FLAT)
    glEnable(GL_COLOR_MATERIAL)
    glClearColor(0.0, 0.0, 0.0, 0.0)
    glEnable(GL_LIGHTING)
    glEnable(GL_LIGHT0)
    glLight(GL_LIGHT0, GL_POSITION, (4.0, 1.0, 6.0))
    glLight(GL_LIGHT0, GL_DIFFUSE, (0.0, 1.0, 0.0))
    glMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (1.0, 1.0, 1.0))

    objects = read_obj('omino.obj')

    frame = 0
    while True:

        # Receive and process events
        for event in pygame.event.get():

            if event.type == pygame.locals.QUIT:
                sys.exit(1)

            if event.type == pygame.locals.VIDEORESIZE:
                resize(event.w, event.h)

        render(frame / FPS)

        #for obj in objects:
        #    obj.draw()

        pygame.display.flip()
        clock.tick(FPS)
        frame += 1

if __name__ == '__main__':
    main()
