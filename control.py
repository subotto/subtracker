import math
import cv2
import logging

class ControlGroup:
    """
    A group of related controls.

    Represents abstractly a groups of controls and holds their configuration.
    It does NOT hold the current values of the controls (see ControlGroupStatus)
    and it is NOT associated to any concrete visualization of them with
    OpenCV (see ControlGroupDisplay).
    """

    def __init__(self):
        self.children = {}

    def window(self, name):
        return self.children.setdefault(name, Window(name))

    def trackbar(self, name, **args):
        return self.children.setdefault(name, Trackbar(name, **args))

    def subpanel(self, name):
        return self.children.setdefault(name, ControlGroup())

    def child(self, name):
        return self.children[name]

    def display(self, namespace = ()):
        return ControlGroupDisplay(namespace, self)

    def create_status(self):
        return ControlGroupStatus(self)

class ControlGroupDisplay:
    """
    A visualization of a group of controls with OpenCV.
    """

    def __init__(self, namespace, group):
        self.group = group
        self.namespace = namespace
        self.children = {}

    def show(self):
        for name, c in self.group.children.items():
            self.child(name).show()

    def child(self, name):
        if not self.children.has_key(name):
            self.children[name] = self.group.child(name).display(self.namespace + (name,))
        return self.children[name]

    def update(self, status):
        """
        Updates the visualization with the values in status.
        """

        for name, c in self.children.items():
            c.update(status.child(name))

    def read(self, status):
        """
        Reads the status of the controls and saves it in status.
        """

        for name, c in self.children.items():
            c.read(status.child(name))

class ControlGroupStatus:
    """
    Values that can be shown or read from a group of controls.
    """

    def __init__(self, group):
        self.group = group
        self.children = {}

    def window(self, name):
        window = self.group.window(name)
        return self.children.setdefault(name, window.create_status())

    def show(self, name, image):
        self.window(name).image = image

    def trackbar(self, name, **args):
        trackbar = self.group.trackbar(name, **args)
        return self.children.setdefault(name, trackbar.create_status())

    def subpanel(self, name):
        subpanel = self.group.subpanel(name)
        return self.children.setdefault(name, subpanel.create_status())

    def child(self, name):
        return self.children.setdefault(name, self.group.child(name).create_status())

class Trackbar:
    """
    A trackbar configured to select a real number in a given interval.

    It is only an abstract representation, similarly to ControlGroup.
    """

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
    """
    The visualization of a trackbar with OpenCV.
    """

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
        self.show()

    def read(self, status):
        pos = cv2.getTrackbarPos(self.name, self.window_name)
        status.set_pos(pos)

    def on_change(self, value):
        pass

class TrackbarStatus:
    """
    Holds a possible value of a trackbar.
    """

    def __init__(self, trackbar):
        self.trackbar = trackbar
        self.value = trackbar.initial_value

    def set_pos(self, pos):
        self.value = self.trackbar.min_value + (pos * self.trackbar.step)

class Window:
    """
    A window used to display an image.

    It is only an abstract representation, similarly to ControlGroup.
    """

    def __init__(self, name):
        self.name = name

    def display(self, namespace):
        return WindowDisplay(namespace, self)

    def create_status(self):
        return WindowStatus(self)

class WindowDisplay:
    """
    The visualization of a window with OpenCV.
    """

    initial_pos = 300
    max_pos = 600

    def __init__(self, namespace, window):
        self.namespace = namespace
        self.window = window

        self.name = " ".join(namespace + (window.name,))

        self.last_image = None
        self.contrast_pos = WindowDisplay.initial_pos
        self.brightness_pos = WindowDisplay.initial_pos
        self.process_trackbar_pos()

        self.shown = False

    def show(self):
        if not self.shown:
            cv2.namedWindow(self.name)
            cv2.createTrackbar("contrast", self.name, self.contrast_pos, WindowDisplay.max_pos, self.on_trackbar_change)
            cv2.createTrackbar("brightness", self.name, self.brightness_pos, WindowDisplay.max_pos, self.on_trackbar_change)
            self.shown = True

    def update(self, status):
        self.show()
        self.last_image = status.image
        self.update_image()

    def update_image(self):
        if self.last_image is not None:
            disp = self.last_image * self.contrast + self.brightness
            cv2.imshow(self.name, disp)

    def read(self, status):
        pass

    def trackbar_pos_to_contrast(self, pos):
        diff = pos - WindowDisplay.initial_pos
        return math.pow(10, diff/100.0)

    def trackbar_pos_to_brightness(self, pos):
        diff = pos - WindowDisplay.initial_pos
        return math.copysign(math.pow(10, abs(diff/100.0) - 1.0), diff)

    def process_trackbar_pos(self):
        self.brightness = self.trackbar_pos_to_brightness(self.brightness_pos)
        self.contrast = self.trackbar_pos_to_contrast(self.contrast_pos)

    def on_trackbar_change(self, ignored_value):
        self.brightness_pos = cv2.getTrackbarPos("brightness", self.name)
        self.contrast_pos = cv2.getTrackbarPos("contrast", self.name)

        self.process_trackbar_pos()
        self.update_image()

class WindowStatus:
    """
    Holds the image that can be shown in a window.
    """

    def __init__(self, image):
        self.image = None


class ControlPanel:
    """
    Manages the visualization and the content of all the controls during
    real-time or offline analysis of a video.
    """

    key_map = {
        "h": ("help", )
    }

    def __init__(self):
        self.controls = ControlGroup()
        self.display = self.controls.display()

    def on_key_pressed(self, key):
        action = ControlPanel.key_map.get(key, ("unbound_key", key))
        getattr(self, action[0])(*action[1:])

    def create_frame_controls(self):
        status = self.controls.subpanel("frame analysis").create_status()
        self.display.child("frame analysis").read(status)
        return status

    def on_new_analysis(self, analysis):
        self.display.show()
        self.display.child("frame analysis").update(analysis.controls)

    def unbound_key(self, key):
        logging.info("Unbound key: %s" % (key,))

    def help(self):
        logging.info("TODO: keys documentation goes here (?)")

if __name__ == "__main__":
    test()
