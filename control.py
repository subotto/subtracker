import math
import cv2

class ControlPanel:

    def __init__(self):
        self.children = {}

    def window(self, name):
        return self.children.setdefault(name, Image(name))

    def trackbar(self, name, **args):
        return self.children.setdefault(name, Trackbar(name, **args))

    def subpanel(self, name):
        return self.children.setdefault(name, ControlPanel())

    def child(self, name):
        return self.children[name]

    def display(self, namespace):
        return ControlPanelDisplay(namespace, self)

    def create_status(self):
        return ControlPanelStatus(self)

class ControlPanelDisplay:

    def __init__(self, namespace, panel):
        self.panel = panel
        self.namespace = namespace
        self.children = {}

    def show(self):
        for name, c in self.panel.children.items():
            if not self.children.has_key(name):
                self.children[name] = c.display(self.namespace + (name,))

        for c in self.children.values():
            c.show()

    def update(self, context):
        for name, c in self.children.items():
            c.update(context.child(name))

    def read(self, context):
        for name, c in self.children.items():
            c.read(context.child(name))

class ControlPanelStatus:

    def __init__(self, panel):
        self.panel = panel
        self.children = {}

    def window(self, name):
        image = self.panel.window(name)
        return self.children.setdefault(name, image.create_status())

    def show(self, name, image):
        self.window(name).image = image

    def trackbar(self, name, **args):
        trackbar = self.panel.trackbar(name, **args)
        return self.children.setdefault(name, trackbar.create_status())

    def subpanel(self, name):
        subpanel = self.panel.subpanel(name)
        return self.children.setdefault(name, subpanel.create_status())

    def child(self, name):
        return self.children.setdefault(name, self.panel.child(name).create_status())

class Trackbar:

    def __init__(self, name, window_name = "", initial_value = 0.0, min_value = 0.0, max_value = 1.0, step = 0.01):
        self.name = name
        self.window_name = window_name

        self.initial_value = initial_value
        self.min_value = min_value
        self.max_value = max_value
        self.step = 0.01

        self.max_pos = int((max_value - min_value) / step)
        self.initial_pos = int((initial_value - min_value) / step)

    def display(self, namespace):
        return TrackbarDisplay(namespace, self)

    def create_status(self):
        return TrackbarStatus(self)

class TrackbarDisplay:

    def __init__(self, namespace, trackbar):
        self.namespace = namespace
        self.trackbar = trackbar
        self.name = trackbar.name
        self.window_name = " ".join(namespace + (trackbar.window_name,))

        self.shown = False

    def show(self):
        if not self.shown:
            t = self.trackbar
            cv2.namedWindow(self.window_name)
            cv2.createTrackbar(self.name, self.window_name, t.initial_pos, t.max_pos, self.on_change)
            self.shown = True

    def update(self, status):
        pass

    def read(self, status):
        pos = cv2.getTrackbarPos(self.name, self.window_name)
        status.set_pos(pos)

    def on_change(self, value):
        pass

class TrackbarStatus:

    def __init__(self, trackbar):
        self.trackbar = trackbar
        self.value = trackbar.initial_value

    def set_pos(self, pos):
        self.value = self.trackbar.min_value + (pos * self.trackbar.step)

class Image:

    def __init__(self, name):
        self.name = name

    def display(self, namespace):
        return ImageDisplay(namespace, self)

    def create_status(self):
        return ImageStatus(self)

class ImageDisplay:

    initial_pos = 300
    max_pos = 600

    def __init__(self, namespace, image):
        self.namespace = namespace
        self.image = image

        self.name = " ".join(namespace + (image.name,))

        self.last_image = None
        self.contrast_pos = ImageDisplay.initial_pos
        self.brightness_pos = ImageDisplay.initial_pos
        self.process_trackbar_pos()

        self.shown = False

    def show(self):
        if not self.shown:
            cv2.namedWindow(self.name)
            cv2.createTrackbar("contrast", self.name, self.contrast_pos, ImageDisplay.max_pos, self.on_trackbar_change)
            cv2.createTrackbar("brightness", self.name, self.brightness_pos, ImageDisplay.max_pos, self.on_trackbar_change)
            self.shown = True

    def update(self, status):
        self.last_image = status.image
        self.update_image()

    def update_image(self):
        if self.last_image is not None:
            disp = self.last_image * self.contrast + self.brightness
            cv2.imshow(self.name, disp)

    def read(self, status):
        pass

    def trackbar_pos_to_contrast(self, pos):
        diff = pos - ImageDisplay.initial_pos
        return math.pow(10, diff/100.0)

    def trackbar_pos_to_brightness(self, pos):
        diff = pos - ImageDisplay.initial_pos
        return math.copysign(math.pow(10, abs(diff/100.0) - 1.0), diff)

    def process_trackbar_pos(self):
        self.brightness = self.trackbar_pos_to_brightness(self.brightness_pos)
        self.contrast = self.trackbar_pos_to_contrast(self.contrast_pos)

    def on_trackbar_change(self, ignored_value):
        self.brightness_pos = cv2.getTrackbarPos("brightness", self.name)
        self.contrast_pos = cv2.getTrackbarPos("contrast", self.name)

        self.process_trackbar_pos()
        self.update_image()

class ImageStatus:

    def __init__(self, image):
        # FIXME: name clash 'image'
        self.image = None

class FrameAnalysisPlayback:

    def __init__(self, panel):
        self.panel = panel
        self.display = panel.display(())

    def create_panel(self):
        panel = ControlPanelStatus(self.panel)
        self.display.read(panel)
        return panel

    def on_new_analysis(self, analysis):
        self.display.show()
        self.display.update(analysis.panel)

class MainPanel(ControlPanel):
    key_map = {
        " ": ("clear_all_panels", )
    }

    def on_key_pressed(self, key):
        action = ControlPanel.key_map[key]
        getattr(self, action[0])(*action[1:])

    def __init__(self):
        pass

def test():
    import random

    class FrameAnalysisStub:

        def __init__(self, panel):
            self.panel = panel

    panel = ControlPanel()
    frames = FrameAnalysisPlayback(panel)

    while cv2.waitKey(0) != "q":
        frame_panel = frames.create_panel()
        analysis = FrameAnalysisStub(frame_panel)

        frame_panel.show("test1", cv2.imread("data/ref.png") * random.gauss(1, 0.1))
        coeff = frame_panel.subpanel("test2").trackbar("coeff", window_name = "settings", initial_value = 0.5).value
        frame_panel.subpanel("test2").show("test3", cv2.imread("data/ref.png") * coeff)

        frames.on_new_analysis(analysis)

if __name__ == "__main__":
    test()
