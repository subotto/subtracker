#!/usr/bin/python
# coding=utf8

import numpy
import random

import metrics


class ParticleFilterSettings:
    
    def __init__(self):
        num_particles = 10


class ModelSettings:
    
    def __init__(self):
        # Geometric parameters
        self.xmin = -metrics.FIELD_WIDTH/2
        self.xmax = metrics.FIELD_WIDTH/2
        self.ymin = -metrics.FIELD_HEIGHT/2
        self.ymax = metrics.FIELD_HEIGHT/2
        
        # Prior on the ball state (position: uniform distribution; velocity: gaussian distribution)
        self.presence_probability = 0.9  # The ball is present on the field
        self.velocity_variance = 0.1
        self.velocity_covariance = numpy.diag([self.velocity_variance]*2)
        
        # Motion parameters
        self.avg_absence_time = 10.0    # Absence is exponential (we assume that the framerate is small)
        self.disappear_probability_per_second = 0.01    # First order approximation
        self.hit_probability_per_second = 1.0   # First order approximation
        self.position_error_variance = 0.001
        self.position_error_covariance = numpy.diag([self.position_error_variance]*2)
        self.velocity_error_variance = 1.0
        self.velocity_error_covariance = numpy.diag([self.velocity_error_variance]*2)


class Particle:
    def __init__(self, weight, position=None, velocity=None):
        self.weight = weight
        self.position = position
        self.velocity = velocity
    
    
    def random_init(self, model_settings):
        """
        Initialize position and velocity randomly, according to the prior given in the settings.
        """
        x_position = random.uniform(self.model_settings.xmin, self.model_settings.xmax)
        y_position = random.uniform(self.model_settings.ymin, self.model_settings.ymax)
        self.position = numpy.array([x_position, y_position])
        self.velocity = numpy.random.multivariate_normal(0.0, self.model_settings.velocity_covariance)
    
    
    def is_present(self):
        """
        Returns whether the ball is present on the field.
        """
        return self.position is not None
    
    
    def evolve(self, timedelta, model_settings):
        """
        Evolve the particle randomly, according to the given model settings.
        """
        if self.is_present():
            # The ball is present
            if random.random() < model_settings.disappear_probability_per_second * timedelta:
                # The ball disappears
                self.position = None
                self.velocity = None
            
            else:
                # The ball remains in the field
                
                # Update position
                self.position += self.velocity * timedelta + numpy.random.multivariate_normal(0.0, model_settings.position_error_covariance)
                
                # Update velocity
                if random.random() < model_settings.hit_probability_per_second * timedelta:
                    # The ball is hit, so velocity is sampled again
                    self.velocity = numpy.random.multivariate_normal(0.0, model_settings.velocity_covariance)
                else:
                    # The ball is not hit
                    self.velocity += numpy.random.multivariate_normal(0.0, model_settings.velocity_error_covariance)
        
        else:
            # The ball is absent
            if random.random() < numpy.exp(-timedelta/model_settings.abg_absence_time):
                # The ball remains absent
                pass
            
            else:
                # The ball appears
                self.random_init(model_settings)
    
    
    def update_weight(self, ballness, model_settings):
        """
        Update the importance weight, according to what is observed.
        """
        # TODO: implement
        pass


class ParticleFilter:
    
    def __init__(self, settings=ParticleFilterSettings(), model_settings=ModelSettings()):
        self.particles = []
        self.time = None
        
        self.settings = settings
        self.model_settings = model_settings
        
        self.spawn_particles()
    
    
    def process_frame(self, ballness, new_time):
        pass
    
    
    def spawn_particles(self):
        """
        Initialize particles according to the given settings.
        """
        weight = 1.0/self.settings.num_particles
        
        for i in xrange(self.settings.num_particles):
            if random.random() > self.model_settings.presence_probability:
                # The ball is not present on the field
                self.particles.append(Particle(weight=weight))
            
            else:
                # The ball is present on the field
                particle = Particle(weight=weight)
                particle.random_init(self.model_settings)
                self.particles.append(particle)
    
    
    def do_sampling_step(self, ballness, new_time):
        """
        Importance sampling step of Bootstrap Filter.
        See http://www.cs.ubc.ca/%7Earnaud/doucet_defreitas_gordon_smcbookintro.ps (page 11).
        """
        for particle in self.particles:
            particle.evolve(new_time - self.time, self.model_settings)
            particle.update_weight(ballness, self.model_settings)
            
        weights_sum = sum([particle.weight for particle in self.particles])
        for particle in self.particles:
            particle.weight /= weights_sum
    
    
    def do_selection_step(self):
        """
        Selection step of Bootstrap Filter.
        See http://www.cs.ubc.ca/%7Earnaud/doucet_defreitas_gordon_smcbookintro.ps (page 11).
        """
        # TODO
        pass


