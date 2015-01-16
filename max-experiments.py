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
        self.frame_num = 120
        self.frame_size = (240, 320)
        self.channels = 3

        sh = metrics.FIELD_HEIGHT/2
        sw = metrics.FIELD_WIDTH/2
        self.table_corners = np.float32([
            (-sw, -sh),
            (-sw, +sh),
            (+sw, +sh),
            (+sw, -sh),
        ])
        
        # estimate background a margin beyond the end of the table
        margin = 0.05
        bsw = sw + margin
        bsh = sh + margin
        
        self.background_corners = np.float32([
            (-bsw, -bsh),
            (-bsw, +bsh),
            (+bsw, +bsh),
            (+bsw, -bsh),
        ])
        
        # pixel/meter
        self.background_resolution = 200
        r = self.background_resolution
        self.background_size = (int(2*bsh*r), int(2*bsw*r))
        self.background_transform = rectangle_to_pixels(*self.background_size[::-1]).dot(region_to_rectangle(self.background_corners))
        
        self.occlusion_mean_diameter = 0.05 # 5cm
        self.occlusion_mean_duration = 2 # 2sec
        
        # use the last 4 bits in integer pixel coordinates for decimals (sub-pixel)
        self.shift = 4


class BlockAnalysis:
    
    def __init__(self, settings):
        self.settings = settings    
    
        f = settings.frame_num
        h, w = settings.frame_size
        ch = settings.channels
    
        self.frame = np.empty((f, h, w, ch), dtype=np.uint8)
        self.frame_32f = np.empty((f, h, w, ch), dtype=np.float32)
        
        self.frame_table_corners = np.empty((f, 4, 2), dtype=np.float32)
        self.frame_transform = np.empty((f, 3, 3), dtype=np.float32)
        self.frame_table_mask_u8 = np.zeros((f, h, w, ch), dtype=np.uint8)
        self.frame_table_mask = np.zeros((f, h, w, ch), dtype=np.float32)
    
        bh, bw = settings.background_size
        
        self.frame_to_background_transform = np.empty((f, 3, 3), dtype=np.float32)
        self.background_sample = np.empty((f, bh, bw, ch), dtype=np.float32)
        
        self.background_mean = np.empty((bh, bw, ch), dtype=np.float32)
        self.background_occlusion_mask = np.empty((f, bh, bw, 1), dtype=np.float32)
        self.background_color_variance = np.empty((3, 3), dtype=np.float32)
        
        self.frame_background_mean = np.empty((f, h, w, ch), dtype=np.float32)
        self.frame_background_deviation = np.empty((f, h, w, ch), dtype=np.float32)
        self.frame_background_nll = np.empty((f, h, w), dtype=np.float32)

def capture(block, cap):
    for f in xrange(settings.frame_num):
        if not cap.read(block.frame[f])[0]:
            return
    block.frame_32f[:] = block.frame / 255.0

def track_table(block):
    prev_tracker = None
    for f in xrange(block.settings.frame_num):
        tracker = TableTracker(prev_tracker, TableTrackingSettings(), controls)
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
            block.settings.background_size[::-1],
            block.background_sample[f],
            cv2.INTER_AREA)

def initialize_occlusion_mask(block):
    f, bw, bh, ch = block.background_sample.shape    
    block.background_occlusion_mask[:] = np.ones((f, bw, bh, 1), dtype=np.float32)

def estimate_background_mean(block):
    block.background_mean[:] = np.average(block.background_sample, 0)

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

def compute_background_nll(block):
    k = block.settings.channels
    deviation = block.frame_background_deviation
    sigma = block.background_color_variance[:]
    sigma_inv = np.linalg.inv(sigma)
    
    # PDF of multivariate normal distribution (see Wikipedia)
    block.frame_background_nll[:] = 0.5 * (
        2*np.pi*k +
        np.linalg.det(sigma) +
        np.einsum("...i,...ij,...j", deviation, sigma_inv, deviation))

def analyze_block(settings, cap):
    block = BlockAnalysis(settings)

    capture(block, cap)
    track_table(block)
    generate_frame_table_mask(block)
    compute_frame_to_background_transform(block)
    sample_background(block)
    estimate_background_mean(block)
    subtract_background_mean(block)
    estimate_background_color_variance(block)
    compute_background_nll(block)

    #block.background_outlier_mask[:,:,:,0] = sp.ndimage.filters.uniform_filter(np.sum(deviation ** 2, 3))
    #block.background_mean[:] = np.sum(block.background_sample * block.background_outlier_mask, 0) / np.sum(block.background_outlier_mask, 0)
    
    return block
        
import sys

def show(name, image):
    cv2.namedWindow(name, cv2.WINDOW_NORMAL)
    cv2.imshow(name, image)

settings = Settings()
cap = cv2.VideoCapture("data/video.mp4")

def move_to_block(index):
    property_pos_frames = 1 # OpenCV magic number
    cap.set(property_pos_frames, settings.frame_num * index)

f = 0
block_index = 2
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
        show("frame_background_nll", block.frame_background_nll[f] / 50)

        key_code = cv2.waitKey()    
        key = chr(key_code & 255)

        if key == ".":
            f += 1
        if key == ",":
            f -= 1
        if key == "q":
            sys.exit(0)
            
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
        

