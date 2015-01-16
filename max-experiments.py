import numpy as np
import scipy as sp
import scipy.ndimage
import cv2

from transformation import *
from control import ControlGroup, ControlGroupStatus
from tabletracker import TableTracker, TableTrackingSettings
import metrics

controls = ControlGroupStatus(ControlGroup())

class Settings:

    def __init__(self):
        self.frame_rate = 120
        self.frame_num = 60
        self.frame_size = (240, 320)
        self.channels = 3

        sh = metrics.FIELD_HEIGHT/2
        sw = metrics.FIELD_WIDTH/2
        self.table_corners = np.float32([
            (-sw, -sh),
            (+sw, -sh),
            (+sw, +sh),
            (-sw, +sh),
        ])
        
        # estimate background a margin beyond the end of the table
        margin = 0.05
        bsw = sw + margin
        bsh = sh + margin
               
        r = 0.005 # meters
        
        self.background_resolution = (int(2*bsw/r), int(2*bsh/r))
        self.background_transform = rectangle_to_pixels(*self.background_resolution).dot(scale(1/bsw, 1/bsh))
        
        self.occlusion_mean_diameter = 0.05 # 5cm
        self.occlusion_mean_duration = 2 # 2sec
        
        self.foosmen_color_mean = np.float32(metrics.FOOSMEN_COLORS)
        self.foosmen_color_variance = 0.1 ** 2 * np.eye(3)

        self.foosmen_overlay_size = (3, 3) # hard-coded in pixels (FIXME?)
        
        teams = 2
        rods = 4
        self.rod_transform = np.empty((teams, rods, 3, 3), dtype=np.float32)
        
        for team in xrange(teams):
            for rod in xrange(rods):
                team_transform = scale((+1, -1)[team], 1)
                rod_location_factor = metrics.ROD_CONFIGURATION[rod][2]
                rod_location_transform = translate(rod_location_factor * metrics.ROD_DISTANCE, 0)
                
                self.rod_transform[team,rod] = team_transform.dot(rod_location_transform)
        
        r = 0.01 # meters
        self.foosmen_location_resolution = (1, int(2*sh/r))
        self.foosmen_location_transform = rectangle_to_pixels(*self.foosmen_location_resolution).dot(scale(1/(4*r), 1/sh))
        
        # use the last 4 bits in integer pixel coordinates for decimals (sub-pixel)
        self.shift = 4


class BlockAnalysis:
    
    def __init__(self, settings):
        self.settings = settings    
    
        f = settings.frame_num
        h, w = settings.frame_size
        ch = settings.channels
        rgb = 3
    
        self.frame = np.empty((f, h, w, ch), dtype=np.uint8)
        self.frame_32f = np.empty((f, h, w, ch), dtype=np.float32)
        
        self.frame_table_corners = np.empty((f, 4, 2), dtype=np.float32)
        self.frame_transform = np.empty((f, 3, 3), dtype=np.float32)
        self.frame_table_mask_u8 = np.zeros((f, h, w, ch), dtype=np.uint8)
        self.frame_table_mask = np.zeros((f, h, w, ch), dtype=np.float32)
    
        bw, bh = settings.background_resolution
        
        self.frame_to_background_transform = np.empty((f, 3, 3), dtype=np.float32)
        self.background_sample = np.empty((f, bh, bw, ch), dtype=np.float32)
        
        self.background_mean = np.empty((bh, bw, ch), dtype=np.float32)
        self.background_occlusion_mask = np.empty((f, bh, bw, 1), dtype=np.float32)
        self.background_color_variance = np.empty((3, 3), dtype=np.float32)
        
        # ll := log likelihood
        # llr := log likelihood ratio
        
        self.frame_background_mean = np.empty((f, h, w, ch), dtype=np.float32)
        self.frame_background_deviation = np.empty((f, h, w, ch), dtype=np.float32)
        self.frame_background_pixel_ll = np.empty((f, h, w, 1), dtype=np.float32)
        
        teams = 2
        rods = 4
        
        self.frame_foosmen_debug = np.zeros((teams, rods, f, h, w, rgb), dtype=np.float32)
        
        self.frame_foosmen_pixel_ll = np.empty((teams, f, h, w, 1), dtype=np.float32)
        
        # ratio of likelihoods of a foosman to be located or not
        # in the center of a frame pixel, given the frame
        self.frame_foosmen_llr = np.empty((teams, f, h, w, 1), dtype=np.float32)
        
        fw, fh = settings.foosmen_location_resolution
        
        self.foosmen_location_llr = np.empty((teams, rods, f, fh, fw, 1), dtype=np.float32)
        self.foosmen_pattern = np.empty((teams, rods, f, fh, fw, 1), dtype=np.float32)
        self.foosmen_centered_pattern = np.empty((teams, rods, f, fh, fw, 1), dtype=np.float32)
        self.foosmen_location_density = np.empty((teams, rods, f, fh, fw, 1), dtype=np.float32)

def capture(block, cap):
    for f in xrange(settings.frame_num):
        if not cap.read(block.frame[f])[0]:
            return
    block.frame_32f[:] = block.frame / 255.0

def track_table(block):
    prev_tracker = None
    for f in xrange(block.settings.frame_num):
        tracker = TableTracker(prev_tracker, TableTrackingSettings(False, None, None), controls)
        prev_tracker = tracker
        block.frame_table_corners[f] = tracker.track_table(block.frame[f]).reshape(4, 2)
        block.frame_transform[f] = cv2.getPerspectiveTransform(block.settings.table_corners, block.frame_table_corners[f])

def generate_frame_table_mask(block):
    shift = block.settings.shift
    corners = np.int32(block.frame_table_corners * 2**shift)
    for f in xrange(block.settings.frame_num):
        cv2.fillConvexPoly(block.frame_table_mask_u8[f], corners[f], (255,) * settings.channels, cv2.CV_AA, shift)
    block.frame_table_mask[:] = block.frame_table_mask_u8 / 255.0

def compute_frame_to_background_transform(block):
    frame_transform_inv = np.linalg.inv(block.frame_transform)
    block.frame_to_background_transform = settings.background_transform.dot(frame_transform_inv).transpose(1,0,2)

def sample_background(block):
    for f in xrange(block.settings.frame_num):
        cv2.warpPerspective(
            block.frame_32f[f],
            block.frame_to_background_transform[f],
            block.settings.background_resolution,
            block.background_sample[f],
            cv2.INTER_AREA)

def initialize_occlusion_mask(block):
    f, bw, bh, ch = block.background_sample.shape
    block.background_occlusion_mask[:] = np.ones((f, bw, bh, 1), dtype=np.float32)

def estimate_background_mean(block):
    block.background_mean[:] = np.sum(block.background_sample * block.background_occlusion_mask, 0) / np.sum(block.background_occlusion_mask, 0)

def subtract_background_mean(block):
    for f in xrange(block.settings.frame_num):
        cv2.warpPerspective(
            block.background_mean,
            block.frame_to_background_transform[f],
            block.settings.frame_size[::-1],
            block.frame_background_mean[f],
            cv2.WARP_INVERSE_MAP | cv2.INTER_AREA)    
    block.frame_background_deviation[:] = (block.frame_32f - block.frame_background_mean) * block.frame_table_mask

def estimate_background_color_variance(block):
    block.background_color_variance[:] = np.tensordot(block.frame_background_deviation, block.frame_background_deviation, ((0,1,2),(0,1,2))) / np.sum(block.frame_table_mask)

def gaussian_log_pdf(k, deviation, sigma):
    sigma_inv = np.linalg.inv(sigma)
    
    return -0.5 * (
            2*np.pi*k +
            np.linalg.det(sigma) +
            np.einsum("...i,...ij,...j", deviation, sigma_inv, deviation))

def compute_background_pixel_ll(block):
    k = block.settings.channels
    deviation = block.frame_background_deviation
    sigma = block.background_color_variance[:]
    
    block.frame_background_pixel_ll[:,:,:,0] = gaussian_log_pdf(k, deviation, sigma)

def draw_foosmen_debug(block):
    for f in xrange(block.settings.frame_num):
        for team in range(2):
            for rod in range(4):
                out = block.frame_foosmen_debug[team,rod,f]
                color = ((0,0,1), (1,0,0))[team]
                transform = block.frame_transform[f].dot(block.settings.rod_transform[team,rod])
                p1 = tuple(int(x) for x in transform.dot((0, -0.3, 1))[:2])
                p2 = tuple(int(x) for x in transform.dot((0, +0.3, 1))[:2])
                cv2.line(out, p1, p2, color, 2, cv2.CV_AA)

def compute_foosmen_pixel_ll(block):
    k = block.settings.channels
    
    for team in xrange(2):
        mean = block.settings.foosmen_color_mean[team]
        deviation = block.frame_32f - mean
        sigma = block.settings.foosmen_color_variance
        block.frame_foosmen_pixel_ll[team,:,:,:,0] = gaussian_log_pdf(k, deviation, sigma)

def compute_foosmen_llr(block):
    size = block.settings.foosmen_overlay_size
    foreground_ll = block.frame_foosmen_pixel_ll
    background_ll = block.frame_background_pixel_ll
    filter_size = (1, 1,) + size + (1,) # team, frame, h, w, channels
    sp.ndimage.uniform_filter(foreground_ll - background_ll, filter_size, block.frame_foosmen_llr)

def sample_foosmen_llr(block):
    frame_transform_inv = np.linalg.inv(block.frame_transform)
    for team in xrange(2):
        for rod in xrange(4):
            rod_transform_inv = np.linalg.inv(block.settings.rod_transform[team,rod])
            
            transform = block.settings.foosmen_location_transform.dot(rod_transform_inv).dot(frame_transform_inv).transpose(1,0,2) # FIXME: document this transpose...
    
            for f in xrange(block.settings.frame_num):
                block.foosmen_location_llr[team,rod,f,:,:,0] = cv2.warpPerspective(block.frame_foosmen_llr[team,f], transform[f], block.settings.foosmen_location_resolution)

def analyze_block(settings, cap):
    block = BlockAnalysis(settings)

    capture(block, cap)
    track_table(block)
    generate_frame_table_mask(block)
    compute_frame_to_background_transform(block)
    sample_background(block)
    initialize_occlusion_mask(block)
    estimate_background_mean(block)
    subtract_background_mean(block)
    estimate_background_color_variance(block)
    compute_background_pixel_ll(block)
    
    draw_foosmen_debug(block)
    compute_foosmen_pixel_ll(block)
    compute_foosmen_llr(block)
    sample_foosmen_llr(block)

    #block.background_outlier_mask[:,:,:,0] = sp.ndimage.filters.uniform_filter(np.sum(deviation ** 2, 3))
    #block.background_mean[:] = np.sum(block.background_sample * block.background_outlier_mask, 0) / np.sum(block.background_outlier_mask, 0)
    
    return block
        
import sys

def show(name, image):
    cv2.namedWindow(name, cv2.WINDOW_NORMAL)
    cv2.imshow(name, image)

settings = Settings()
cap = cv2.VideoCapture("data/video.mp4")

show("foosmen_colors", settings.foosmen_color_mean)

def move_to_block(index):
    property_pos_frames = 1 # OpenCV magic number
    cap.set(property_pos_frames, settings.frame_num * index)

f = 0
block_index = 10
team = 0
rod = 0
while True:
    move_to_block(block_index)
    
    block = analyze_block(settings, cap)

    show("background_mean", block.background_mean)

    print "Block: ", block_index

    while True:
        print "Frame: ", f
            
        show("frame", block.frame[f])
        show("frame_table_mask", block.frame_table_mask[f])
        show("background_sample", block.background_sample[f])
        show("frame_background_mean", block.frame_background_mean[f])
        show("frame_background_deviation", block.frame_background_deviation[f] ** 2 * 50)
        show("frame_background_pixel_ll", -block.frame_background_pixel_ll[f] / 30)
        
        show("frame_foosmen_pixel_ll", 1.0 + block.frame_foosmen_pixel_ll[team,f] / 30 + block.frame_foosmen_debug[team,rod,f] * 100)
        show("frame_foosmen_llr", 1.0 + block.frame_foosmen_llr[team,f] / 30 + block.frame_foosmen_debug[team,rod,f] * 100)
        
        show("foosmen_location_llr", cv2.resize(1.0 + block.foosmen_location_llr[team,rod,f] / 30, (30, 200)))
        
        key_code = cv2.waitKey()    
        key = chr(key_code & 255)

        if key == ".":
            f += 1
        if key == ",":
            f -= 1
        if key == "q":
            sys.exit(0)
        if key == "t":
            team = (team+1) % 2
            print "Team: ", team
        if key == "r":
            rod = (rod+1) % 4
            print "Rod: ", rod
            
        if key == "<":
            block_index -= 1
            f = settings.frame_num - 1
            break
        if key == ">":
            block_index += 1
            f = 0
            break
            
        if f >= settings.frame_num:
            f = settings.frame_num - 1
        if f < 0:
            f = 0
        

