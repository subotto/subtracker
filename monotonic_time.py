#!/usr/bin/env python2

# Taken from http://stackoverflow.com/a/1205762/807307

__all__ = ["monotonic_time"]

import ctypes, os, sys, time

CLOCK_MONOTONIC_RAW = 4 # see <linux/time.h>

class timespec(ctypes.Structure):
    _fields_ = [
        ('tv_sec', ctypes.c_long),
        ('tv_nsec', ctypes.c_long)
    ]

if sys.platform.startswith("linux"):
    librt = ctypes.CDLL('librt.so.1', use_errno=True)
    clock_gettime = librt.clock_gettime
    clock_gettime.argtypes = [ctypes.c_int, ctypes.POINTER(timespec)]

def monotonic_time():
    # Linux is supported
    if sys.platform.startswith("linux"):
        t = timespec()
        if clock_gettime(CLOCK_MONOTONIC_RAW , ctypes.pointer(t)) != 0:
            errno_ = ctypes.get_errno()
            raise OSError(errno_, os.strerror(errno_))
        return t.tv_sec + t.tv_nsec * 1e-9

    # All the other platforms are not. Sorry!
    else:
        return time.time()

if __name__ == "__main__":
    print monotonic_time()

