import numpy as np
import cv2

from transformation import *
from control import ControlGroup, ControlGroupStatus
from tabletracker import TableTracker, TableTrackingSettings
import metrics

controls = ControlGroupStatus(ControlGroup())

class Settings:

    def __init__(self):
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
        self.background_mean_resolution = 200
        r = self.background_mean_resolution
        self.background_mean_size = (int(2*bsh*r), int(2*bsw*r))
        self.background_mean_transform = rectangle_to_pixels(*self.background_mean_size[::-1]).dot(region_to_rectangle(self.background_corners))
        
        # use the last 4 bits in integer pixel coordinates for decimals (sub-pixel)
        self.shift = 4


class BlockAnalysis:
    
    def __init__(self, settings):
        f = settings.frame_num
        h, w = settings.frame_size
        ch = settings.channels
    
        self.frame = np.empty((f, h, w, ch), dtype=np.uint8)
        self.frame_32f = np.empty((f, h, w, ch), dtype=np.float32)
        
        self.frame_table_corners = np.empty((f, 4, 2), dtype=np.float32)
        self.frame_table_corners_fixed = np.empty((f, 4, 2), dtype=np.int32)
        self.frame_transform = np.empty((f, 3, 3), dtype=np.float32)
        self.frame_table_mask_u8 = np.zeros((f, h, w, ch), dtype=np.uint8)
        self.frame_table_mask = np.zeros((f, h, w, ch), dtype=np.float32)
    
        bh, bw = settings.background_mean_size
        self.background_sample = np.empty((f, bh, bw, ch), dtype=np.float32)
        self.background_mean = np.empty((bh, bw, ch), dtype=np.float32)
        
        self.frame_background_mean = np.empty((f, h, w, ch), dtype=np.float32)
        self.frame_background_deviation = np.empty((f, h, w, ch), dtype=np.float32)

def analyze_block(settings, cap):
    block = BlockAnalysis(settings)

    fn = settings.frame_num
    for f in xrange(fn):
        if not cap.read(block.frame[f])[0]:
            break
            
    block.frame_32f[:] = block.frame / 255.0

    prev_tracker = None
    for f in xrange(fn):
        tracker = TableTracker(prev_tracker, TableTrackingSettings(), controls)
        prev_tracker = tracker
        block.frame_table_corners[f] = tracker.track_table(block.frame[f]).reshape(4, 2)
    
    for f in xrange(fn):
        block.frame_transform[f] = cv2.getPerspectiveTransform(settings.table_corners, block.frame_table_corners[f])
        
    block.frame_table_corners_fixed[:] = np.int32(block.frame_table_corners * 2**settings.shift)

    for f in xrange(fn):
        cv2.fillConvexPoly(block.frame_table_mask_u8[f], block.frame_table_corners_fixed[f], (255,) * settings.channels, cv2.CV_AA, settings.shift)
    block.frame_table_mask[:] = block.frame_table_mask_u8 / 255.0
        
    frame_transform_inv = np.linalg.inv(block.frame_transform)
    frame_to_background_transform = settings.background_mean_transform.dot(frame_transform_inv).transpose(1,0,2)
    
    for f in xrange(fn):
        cv2.warpPerspective(
            block.frame_32f[f],
            frame_to_background_transform[f],
            settings.background_mean_size[::-1],
            block.background_sample[f],
            cv2.INTER_AREA)
        
    block.background_mean[:] = np.average(block.background_sample, 0)
    
    for f in xrange(fn):
        cv2.warpPerspective(
            block.background_mean,
            frame_to_background_transform[f],
            settings.frame_size[::-1],
            block.frame_background_mean[f],
            cv2.WARP_INVERSE_MAP | cv2.INTER_AREA)
    
    block.frame_background_deviation = (block.frame_32f - block.frame_background_mean) * block.frame_table_mask
    
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
block_index = 0
while True:
    move_to_block(block_index)
    block = analyze_block(settings, cap)

    show("background_mean", block.background_mean)

    print "Block: ", block_index

    while True:
        print "Frame: ", f
            
        show("frame", block.frame[f])
        show("frame_table_mask", block.frame_table_mask[f])
        show("background_mean_sample", block.background_sample[f])
        show("frame_background_mean", block.frame_background_mean[f])
        show("frame_background_deviation", block.frame_background_deviation[f] ** 2 * 50)

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
        

