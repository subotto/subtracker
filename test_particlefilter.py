import numpy
import random
import cv2

from particlefilter import *
import transformation
import metrics


def generate_ballness(w, h):
    return numpy.array([random.random() for i in xrange(w*h)]).reshape((w,h))

pf = ParticleFilter()
w = 500
h = 1000
time = 0.0

transform = numpy.dot(transformation.scale(metrics.FIELD_WIDTH, metrics.FIELD_HEIGHT), transformation.rectangle_to_pixels(w, h))


while True:
    ballness = generate_ballness(w, h)
    img = numpy.array([[[x,x,x] for x in row] for row in ballness]).reshape((w,h,3))
    time += 0.5
    n = pf.settings.num_particles
    
    # Sampling step
    pf.do_sampling_step(ballness, time)
    
    for particle in pf.particles:
        if particle.is_present():
            point =  tuple([int(x) for x in transformation.apply_projectivity(transform, particle.position)])
            cv2.circle(img=img, center=point, radius=15, color=(2.0/n - particle.weight, 2.0/n - particle.weight, 1.0), thickness=3)
    
    print 'Particles at time', time, '(after sampling step)'
    for particle in pf.particles:
        print particle
    
    # Selection step
    pf.do_selection_step()
    
    for particle in pf.particles:
        if particle.is_present():
            point =  tuple([int(x) for x in transformation.apply_projectivity(transform, particle.position)])
            cv2.circle(img=img, center=point, radius=10, color=(0.0, 1.0, 0.0), thickness=3)
    
    print 'Particles at time', time, '(after selection step)'
    for particle in pf.particles:
        print particle
    
    cv2.imshow("Particles", img)
    cv2.waitKey()


