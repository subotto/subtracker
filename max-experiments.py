import numpy as np
import scipy as sp
import scipy.ndimage
import cv2

import transformation
from control import ControlGroup, ControlGroupStatus
from tabletracker import TableTracker, TableTrackingSettings
from table import Table

class Block:

    def __init__(self, table):
        self.frames = 1200

        self.table = table
        self.video = Video(self)
        self.camera = CameraAnalysis(self)
        self.rectification = Rectification(self)
        self.background = BackgroundAnalysis(self)
        self.team_foosmen = [FoosmenSetAnalysis(self, t) for t in self.table.teams]
        self.rods = [RodAnalysis(self, r) for r in self.table.rods]
        self.occlusion = OcclusionAnalysis(self)
        
class Video:

    def __init__(self, block):
        f = block.frames

        self.frame_size = w, h = 320, 240
        self.channels = ch = 3
        self.image = np.empty((f, h, w, ch), np.uint8)
        self.image_f = np.empty((f, h, w, ch), np.float32)

class CameraAnalysis:

    def __init__(self, block):
        f = block.frames
        w, h = block.video.frame_size
        
        self.transform = np.empty((f, 3, 3))
        self.resolution = np.empty((f, h, w), np.float32)

class Rectification:

    def __init__(self, block):
        self.resolution = 80 # pixel/meter
        self.surface_size = (1.4, 0.8)
        self.image_size, self.transform = image_size_transform(self.surface_size, self.resolution)
        
        f = block.frames
        w, h = self.image_size
        self.channels = ch = block.video.channels
        
        self.image = np.empty((f, h, w, ch), np.float32)
        self.camera_resolution = np.empty((f, h, w), np.float32)

class BackgroundAnalysis:

    def __init__(self, block):
        f = block.frames
        w, h = block.rectification.image_size
        ch = block.rectification.channels
        
        # \mu_G
        self.mean = np.empty((h, w, ch), np.float32)
        # \Sigma_G
        self.variance = np.empty((ch, ch), np.float32)
        # I - \mu_G
        self.deviation = np.empty((f, h, w, ch), np.float32)
        
        # LL_G
        self.visible_ll = np.empty((f, h, w), np.float32)
        # LL_{\tilde G}
        self.ll = np.empty((f, h, w), np.float32)

class FoosmenSetAnalysis:

    def __init__(self, block, team):
        self.team = team
    
        # As our model approximates the shape of foosmen as a rectangle
        # of a fixed size, we assume that only a random part of the pixels in
        # this rectangle are actually coming from the foosmen.
        
        # p_{M|\tilde MV}
        self.opaque_p = 0.2
        # p_{G|\tilde MV}
        self.transparent_p = (1-self.opaque_p)
    
        f = block.frames
        w, h = block.rectification.image_size
        ch = block.rectification.channels
        
        # \mu_M
        self.mean = np.float32(team.foosmen.color) * 0.8
        # \Sigma_M
        self.variance = 0.1 ** 2 * np.eye(3, dtype=np.float32) + 0.05 ** 2 * np.ones((3,3), dtype=np.float32)

        # I_i - \mu_M
        self.deviation = np.empty((f, h, w, ch), np.float32)
        
        # LL_M
        self.visible_ll = np.empty((f, h, w), np.float32)
        # LL_{\tilde M}
        self.ll = np.empty((f, h, w), np.float32)
        
        # LLR_{\tilde M:\tilde G}
        self.llr = np.empty((f, h, w), np.float32)
        
        self.silhouette_size = (2*block.table.foosmen.feet_height, block.table.foosmen.width)
        self.silhouette_area = self.silhouette_size[0] * self.silhouette_size[1]
        
        self.filter_size = tuple(s*block.rectification.resolution for s in self.silhouette_size)
        
        # LLR_{C}
        self.location_llr = np.empty((f, h, w), np.float32)
        
        # p(C|I)
        self.location_pd = np.empty((f, h, w), np.float32)
        
        # p(\tilde M|I)
        self.silhouette_p = np.empty((f, h, w), np.float32)
        
        # p(M|I)
        self.visible_p = np.empty((f, h, w), np.float32)

class RodAnalysis:

    def __init__(self, block, rod):
        self.rod = rod
    
        tm = rod.type.translation_max
        self.translation_resolution = tr = 80 # pixel/meter
        
        # Center location moves only along the Y axis,
        # but we approximate this by allowing a small horizontal movement
        # so that transform matrices are not singular and units of measurement
        # make sense more easily
        self.location_delta_width = 0.02
        
        self.translation_size = h = int(2*tm*tr)
        
        # Transforms the translations into the center location
        self.translation_location_transform = get_image_transform((self.location_delta_width, 2*tm), (1, h))
        
        self.foosmen = [FoosmanAnalysis(block, self, m) for m in rod.foosmen]

        f = block.frames
        
        # LLR_S
        self.translation_llr = np.empty((f, h))
        # p(S|I)
        self.translation_pd = np.empty((f, h))

class FoosmanAnalysis:

    def __init__(self, block, rod_analysis, foosman):
        f = block.frames
        h = rod_analysis.translation_size
        
        self.translation_llr = np.empty((f, h))

class OcclusionAnalysis:

    def __init__(self, block):
        # p(O_i)
        self.opaque_p = 0.01
        # p(V_i)
        self.transparent_p = (1 - self.opaque_p)

        f = block.frames
        w, h = block.rectification.image_size
        
        # p(I_i|O_i)
        self.visible_ll = np.empty((f, h, w), np.float32)

from time import time

def timing(f):
    def decorated(*args, **kwargs):
        t = time()
        res = f(*args, **kwargs)
        print "%3.6f time (%s)" % (time() - t, f.__name__)
        return res
    return decorated

@timing
def capture(block, cap):
    for f in xrange(block.frames):
        if not cap.read(block.video.image[f])[0]:
            return
    block.video.image_f[:] = block.video.image / 255.0

@timing
def track_table(block):
    table_corners = get_rectangle_corners(block.table.ground.size)
    
    controls = ControlGroupStatus(ControlGroup())
    prev_tracker = None
    for f in xrange(block.frames):
        tracker = TableTracker(prev_tracker, TableTrackingSettings(False, None, None), controls)
        prev_tracker = tracker
        corners = tracker.track_table(block.video.image[f])
        block.camera.transform[f] = cv2.getPerspectiveTransform(table_corners, corners)

@timing
def compute_resolution(block):
    f = block.frames
    w, h = block.video.frame_size
    
    # We compute the resolution at 4 points, and then interpolate.
    # These are the projective coordinates of 4 points at 1/4 and 3/4
    # of the full width and height, respectively. They were chosen as
    # they are correctly mapped by resizing a 2x2 image using OpenCV
    x = np.array([
        [(1*h/4, 1*w/4, 1), (3*h/4, 1*w/4, 1)],
        [(1*h/4, 3*w/4, 1), (3*h/4, 3*w/4, 1)],
    ])
    
    # Compute the differential of the perspective transform
    A = np.linalg.inv(block.camera.transform)
    Axy = A[...,:-1,:]
    Az = A[...,-1,:]
    term1 = np.einsum("...k,...yxk,...ij->...yxij", Az,  x, Axy)
    term2 = np.einsum("...ik,...yxk,...j->...yxij", Axy,  x, Az)
    z = np.einsum("...i,...yxi->...yx", Az,  x)
    differential = (term1 - term2) / (z**2)[...,np.newaxis,np.newaxis]
    
    # Determinant of differential gives the signed area
    # (in meters**2) covered by each pixel
    pixel_area = np.linalg.det(differential[...,0:2,0:2])

    # Resolution is the inverse of area.    
    # The sign is negative due to different coordinate systems
    # i.e.: pixel Y axis goes up to down,
    # while world Y axis goes down to up.
    # We want a positive sign for computations.
    resolution = -1/pixel_area
    
    # Use OpenCV resize to do the interpolation
    for f in xrange(block.frames):
        block.camera.resolution[f] = cv2.resize(resolution[f], block.video.frame_size, None, cv2.INTER_AREA)

@timing
def rectify(block):
    rec = block.rectification
    transform = np.einsum("...ij,...jk", rec.transform, np.linalg.inv(block.camera.transform))
    for f in xrange(block.frames):
        rec.image[f] = cv2.warpPerspective(block.video.image_f[f], transform[f], rec.image_size)
        rec.camera_resolution[f] = cv2.warpPerspective(block.camera.resolution[f], transform[f], rec.image_size)

@timing
def estimate_background(block):
    rec = block.rectification
    bg = block.background
    bg.mean[:] = np.average(rec.image, 0)
    bg.variance[:] = np.cov(rec.image.reshape(-1, rec.channels), rowvar=False)

@timing
def compute_background_visible_ll(block):
    rec = block.rectification
    bg = block.background
    bg.deviation[:] = rec.image - bg.mean
    # ll is expressed in [meters^-2]
    bg.visible_ll[:] = gaussian_log_pdf(bg.deviation, bg.variance)

@timing
def compute_foosmen_visible_ll(block):
    rec = block.rectification
    for i, team in enumerate(block.table.teams):
        foosmen = block.team_foosmen[i]
        
        foosmen.deviation[:] = rec.image - team.foosmen.color
        foosmen.visible_ll[:] = gaussian_log_pdf(foosmen.deviation, foosmen.variance)

@timing
def compute_occlusion_visible_ll(block):
    # TODO: estimate the likelihood correctly
    
    # The density of the uniform probability on the color space is 1
    block.occlusion.visible_ll[:] = np.log(1)

@timing
def compute_background_ll(block):
    term1 = np.log(block.occlusion.opaque_p) + block.occlusion.visible_ll
    term2 = np.log(block.occlusion.transparent_p) + block.background.visible_ll
    
    block.background.ll[:] = np.log(np.exp(term1) + np.exp(term2))

@timing
def compute_foosmen_ll(block):
    rec = block.rectification
    for i, team in enumerate(block.table.teams):
        foosmen = block.team_foosmen[i]
        
        term1 = np.log(block.occlusion.opaque_p) + block.occlusion.visible_ll
        term2 = np.log(block.occlusion.transparent_p) + np.log(foosmen.opaque_p) + foosmen.visible_ll
        term3 = np.log(block.occlusion.transparent_p) + np.log(foosmen.transparent_p) + block.background.visible_ll
        
        foosmen.ll[:] = np.log(np.exp(term1) + np.exp(term2) + np.exp(term3))

@timing
def compute_foosmen_llr(block):
    for i, team in enumerate(block.table.teams):
        foosmen = block.team_foosmen[i]
        foosmen.llr[:] = foosmen.ll - block.background.ll

@timing
def compute_foosmen_location_llr(block):
    for i, team in enumerate(block.table.teams):
        foosmen = block.team_foosmen[i]
        llr_density = foosmen.llr * block.rectification.camera_resolution
        
        # W, H => F, H, W
        filter_size = (1,) + foosmen.filter_size[::-1]
        foosmen.location_llr[:] = sp.ndimage.uniform_filter(llr_density, filter_size) * foosmen.silhouette_area

@timing
def compute_foosmen_translation_llr(block):
    for i, rod in enumerate(block.table.rods):
        rod_analysis = block.rods[i]
        team_foosmen = block.team_foosmen[rod.team.index]
        
        h = rod_analysis.translation_size
        
        for k, foosman in enumerate(rod.foosmen):
            foosman_analysis = rod_analysis.foosmen[foosman.index]
            transform = np.einsum("...ij,...jk,...kl",
                block.rectification.transform,
                foosman.transform,
                np.linalg.inv(rod_analysis.translation_location_transform))
        
            for f in xrange(block.frames):
                llr = team_foosmen.location_llr[f]
                foosman_analysis.translation_llr[f,...,np.newaxis] = cv2.warpPerspective(llr, transform, (1, h), None, cv2.WARP_INVERSE_MAP)

@timing
def compute_rod_translation_llr(block):
    for i, rod in enumerate(block.table.rods):
        rod_analysis = block.rods[i]
        rod_analysis.translation_llr[:] = sum(f.translation_llr for f in rod_analysis.foosmen)

@timing
def compute_rod_translation_pd(block):
    for i, rod in enumerate(block.table.rods):
        rod_analysis = block.rods[i]
        
        maximum = np.max(rod_analysis.translation_llr, -1, keepdims=True)
        exp = np.exp(rod_analysis.translation_llr - maximum)
        total = np.sum(exp, -1, keepdims=True)
        
        # Normalize density to be in meters^-1
        rod_analysis.translation_pd[:] = exp/total * rod_analysis.translation_resolution

@timing
def compute_foosmen_location_pd(block):
    for i, team in enumerate(block.table.teams):
        block.team_foosmen[i].location_pd[:] = 0
    
    for i, rod in enumerate(block.table.rods):
        rod_analysis = block.rods[i]
        team_foosmen = block.team_foosmen[rod.team.index]
        w = rod_analysis.location_delta_width
        
        for k, foosman in enumerate(rod.foosmen):
            foosman_analysis = rod_analysis.foosmen[foosman.index]
            
            transform = np.einsum("...ij,...jk,...kl",
                block.rectification.transform,
                foosman.transform,
                np.linalg.inv(rod_analysis.translation_location_transform))
        
            for f in xrange(block.frames):
                pd = rod_analysis.translation_pd[f,...,np.newaxis]
                # TODO: avoid aliasing along the Y axis, as it is very relevant
                team_foosmen.location_pd[f] += cv2.warpAffine(pd/w, transform[0:2,:], block.rectification.image_size, None, cv2.INTER_AREA)

@timing
def compute_foosmen_silhouette_p(block):
    for i, team in enumerate(block.table.teams):
        team_foosmen = block.team_foosmen[i]
        density = sp.ndimage.uniform_filter(team_foosmen.location_pd, (1,) + team_foosmen.filter_size[::-1], mode="constant")
        team_foosmen.silhouette_p[:] = density * team_foosmen.silhouette_area

@timing
def compute_foosmen_visible_p(block):
    for i, team in enumerate(block.table.teams):
        team_foosmen = block.team_foosmen[i]
        llr = np.log(block.occlusion.transparent_p) + np.log(team_foosmen.opaque_p) + team_foosmen.visible_ll - block.background.ll
        team_foosmen.visible_p[:] = team_foosmen.silhouette_p * (1 / (1 + np.exp(-llr)))
        
def analyze(block):
    track_table(block)
    compute_resolution(block)
    rectify(block)
    
    estimate_background(block)
    
    compute_background_visible_ll(block)
    compute_foosmen_visible_ll(block)
    compute_occlusion_visible_ll(block)
    
    compute_background_ll(block)
    compute_foosmen_ll(block)
    #compute_occlusion_ll(block)

    compute_foosmen_llr(block)
    compute_foosmen_location_llr(block)
    compute_foosmen_translation_llr(block)

    compute_rod_translation_llr(block)
    compute_rod_translation_pd(block)

    compute_foosmen_location_pd(block)
    compute_foosmen_silhouette_p(block)
    compute_foosmen_visible_p(block)

def gaussian_log_pdf(deviation, sigma):
    if not isinstance(deviation, np.ndarray):
        # make it a vector of length 1
        deviation = np.array([deviation])

    if not isinstance(sigma, np.ndarray):
        # make it a 1x1 matrix
        sigma = np.array([[sigma]])
    
    k = sigma.shape[-1]
    assert sigma.shape[-2] == k
    sigma_inv = np.linalg.inv(sigma)
    
    return -0.5 * (
            k * np.log(2*np.pi) +
            np.log(np.linalg.det(sigma)) +
            np.einsum("...i,...ij,...j", deviation, sigma_inv, deviation))

def get_image_corners(size):
    w, h = size
    return np.float32([
        ( -0.5, h-0.5),
        (w-0.5, h-0.5),
        (w-0.5,  -0.5),
        ( -0.5,  -0.5),
    ], dtype=np.float32)
                        
def get_rectangle_corners(size, origin = None):
    w, h = size

    if origin is None:
        origin = w/2.0, h/2.0

    x, y = origin
    return np.float32([
        ( -x,  -y),
        (w-x,  -y),
        (w-x, h-y),
        ( -x, h-y),
    ])
    
def get_image_transform(surface_size, image_size, origin = None):
    image_corners = get_image_corners(image_size)
    surface_corners = get_rectangle_corners(surface_size, origin)
    return cv2.getPerspectiveTransform(surface_corners, image_corners)
    
def image_size_transform(surface_size, resolution, origin = None):
    if not isinstance(resolution, tuple):
        resolution = resolution, resolution

    image_size = tuple(int(surface_size[i]*resolution[i]) for i in (0,1))
    
    return image_size, get_image_transform(surface_size, image_size, origin)

images = {}
def show(name, image, contrast=1.0, brightness=1.0):
    def on_change(*args):
        c = cv2.getTrackbarPos("contrast", name)
        b = cv2.getTrackbarPos("brightness", name)
        image = images[name]
        cv2.imshow(name, image * 10 ** ((c-1000) / 100.0) + (b-1000)/100.0)

    if cv2.getTrackbarPos("contrast", name) == -1:
        cv2.namedWindow(name, cv2.WINDOW_NORMAL)
        cv2.createTrackbar("contrast", name, int(1000 * contrast), 2000, on_change)
        cv2.createTrackbar("brightness", name, int(1000 * brightness), 2000, on_change)
    images[name] = image
    on_change()

import sys

def move_to_block(index, cap):
    property_pos_frames = 1 # OpenCV magic number
    frames = 1200
    cap.set(property_pos_frames, frames * index)

def run():
    cap = cv2.VideoCapture("data/video.mp4")

    f = 0
    block_index = 10
    
    table = Table()
    team = 0
    rod = 0
    man = 0
    play = False

    while True:
        move_to_block(block_index, cap)
        
        block = Block(table)
        capture(block, cap)
        analyze(block)

        print "Block: ", block_index

        while True:
            print "Frame: ", f
                
            print "Resolution at center: ", block.camera.resolution[f,160,120]
            
            #show("foosmen.color", np.array([[block.table.teams[team].foosmen.color]]))
            
            show("video.image", block.video.image_f[f])
            show("camera.resolution", block.camera.resolution[f])
            show("rectification.image", block.rectification.image[f])
            #show("background.mean", block.background.mean)
            #show("background.deviation", block.background.deviation[f] ** 2)
            
            #show("background.visible_ll", block.background.visible_ll[f])
            #show("team_foosmen.visible_ll", block.team_foosmen[team].visible_ll[f])
            #show("occlusion.visible_ll", block.occlusion.visible_ll[f])
            
            show("background.ll", block.background.ll[f])
            show("team_foosmen.ll", block.team_foosmen[team].ll[f])

            #show("team_foosmen.llr", block.team_foosmen[team].llr[f])
            #show("team_foosmen.location_llr", block.team_foosmen[team].location_llr[f])
            
            rod_index = block.table.teams[team].rods[rod].index
            rod_analysis = block.rods[rod_index]
            foosman_analysis = rod_analysis.foosmen[man]
            #show("foosman.translation_llr", cv2.resize(foosman_analysis.translation_llr[f,...,np.newaxis], (10, rod_analysis.translation_size)))
            #show("rod.translation_llr", cv2.resize(rod_analysis.translation_llr[f,...,np.newaxis], (10, rod_analysis.translation_size)))
            show("rod.translation_pd", cv2.resize(rod_analysis.translation_pd[f,...,np.newaxis], (10, rod_analysis.translation_size)))
            show("team_foosmen.location_pd", block.team_foosmen[team].location_pd[f])
            show("team_foosmen.silhouette_p", block.team_foosmen[team].silhouette_p[f])
            show("team_foosmen.visible_p", block.team_foosmen[team].visible_p[f])
            
            show("Foosmen debug", block.rectification.image[f] + block.team_foosmen[team].location_pd[f,...,np.newaxis]*np.float32([1,0,1]))
            
            if play:
                key_code = cv2.waitKey(10)
                f = (f+1) % block.frames
            else:
                key_code = cv2.waitKey()
                
            key = chr(key_code & 255)

            if key == ".":
                play = False
                f += 1
            if key == ",":
                play = False
                f -= 1
            if key == "q":
                sys.exit(0)
            if key == "t":
                team = (team+1) % 2
                print "Team: ", team
            if key == "e":
                rod = (rod-1) % 4
                man = 0
                print "Rod: ", rod
            if key == "r":
                rod = (rod+1) % 4
                man = 0
                print "Rod: ", rod
                
            if key == "m":
                man = (man-1) % len(block.table.rods.types[rod].foosmen)
                print "Man: ", man
            if key == "n":
                man = (man+1) % len(block.table.rods.types[rod].foosmen)
                print "Man: ", man
                
            if key == "j":
                f = 0
            if key == " ":
                play = not play
                
            if key == "<":
                block_index -= 1
                f = block.frames - 1
                break
            if key == ">":
                block_index += 1
                f = 0
                break
                
            if f >= block.frames:
                f = block.frames - 1
            if f < 0:
                f = 0
            
if __name__ == "__main__":
    run()

