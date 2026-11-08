#ifndef HAILMARY_GST_ELEMENTS_H
#define HAILMARY_GST_ELEMENTS_H

#include <gst/gst.h>
#include <gst/video/video.h>

static const char *CAMERA_DEVICE = "/dev/video0";
static const int CAMERA_WIDTH = 1280;
static const int CAMERA_HEIGHT = 720;
static const int CAMERA_FPS = 30;

class CameraElement {
    public:
        CameraElement();
        ~CameraElement();

        CameraElement(const CameraElement &) = delete;
        CameraElement &operator=(const CameraElement &) = delete;

        bool cameraInit();
        bool cameraConnect(GstElement *bin);

        GstElement *getOutput();

    private:
        GstElement *src;
        GstElement *jpegCaps;
        GstElement *decode;
        GstElement *convert;
        GstElement *output;
        bool hwDecode;
        bool addedToBin;
};

#endif //HAILMARY_GST_ELEMENTS_H
