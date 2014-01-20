#include "v4l2cap.hpp"

#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

static int xioctl(int fh, int request, void *arg) {
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

VideoCapture v4l2cap(int device, int width, int height, int fps) {
    VideoCapture cap = VideoCapture(device);
    if (!cap.isOpened()) {
        fprintf(stderr, "Error opening video capture %d\n", device);
        exit(1);
    }


    cap.set(CV_CAP_PROP_FRAME_WIDTH, width);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, height);

    // Ottengo il file descriptor della webcam
    int fd = -1;
    DIR* dir;
    struct dirent *ent;
    // Possiamo assumere che questo ci sia...
    dir = opendir("/proc/self/fd");
    while ((ent=readdir(dir)) != NULL) {
        // Salta . e ..
        if(ent->d_name[0] == '.') continue;
        char fl[100] = "/proc/self/fd/";
        char dst[100];
        strcat(fl, ent->d_name);
        if(readlink(fl, dst, 100) == -1) {
            fprintf(stderr, "Error listing file descriptors links\n");
            exit(2);
        }
        if(strstr(dst, "/dev/video")) {
            fd = atoi(ent->d_name);
        }
    }
    if(fd == -1) {
        fprintf(stderr, "Cannot find webcam's file descriptor "
                        "- this should not happen!\n");
        exit(3);
    }
    closedir(dir);

    v4l2_streamparm streamparm;
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(fd, VIDIOC_G_PARM, &streamparm) < 0) {
        fprintf(stderr, "Error getting stream parameters!\n");
        exit(4);
    }
    if(!(streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
        fprintf(stderr, "Device does not support setting frame rate!");
        exit(5);
    }
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = fps;
    if(xioctl(fd, VIDIOC_S_PARM, &streamparm) < 0) {
        fprintf(stderr, "Error setting device framerate!\n");
    }
    return cap;
}
