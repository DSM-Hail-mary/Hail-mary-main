#ifndef HAILMARY_GST_ELEMENTS_H
#define HAILMARY_GST_ELEMENTS_H

#include <gst/gst.h>
#include <gst/video/video.h>
#include "main_pipeline.h"

static const char *OPENCV_HOST = "127.0.0.1";
static const int OPENCV_PORT = 5000;
static const int OPENCV_WIDTH = 1280;
static const int OPENCV_HEIGHT = 720;
static const int OPENCV_FPS = 30;

class CameraElement {
    public:
        CameraElement();
        ~CameraElement();

        CameraElement(const CameraElement &) = delete;
        CameraElement &operator=(const CameraElement &) = delete;

        bool cameraInit();
        bool cameraConnect();

        GstElement *getOutput();

    private:
        GstElement *src;
        GstElement *parse;
        bool addedToBin;
};

#endif //HAILMARY_GST_ELEMENTS_H
