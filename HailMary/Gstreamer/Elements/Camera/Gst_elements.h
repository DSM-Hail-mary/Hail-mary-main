#ifndef HAILMARY_GST_ELEMENTS_H
#define HAILMARY_GST_ELEMENTS_H

#include <gst/gst.h>
#include "main_pipeline.h"

#ifdef _WIN32
static const char *CAMERA_SRC = "mfvideosrc";
#else
static const char *CAMERA_SRC = "v4l2src";
#endif

class CameraElement {
    public:
        CameraElement();
        ~CameraElement();

        CameraElement(const CameraElement &) = delete;
        CameraElement &operator=(const CameraElement &) = delete;

        bool cameraInit();
        bool cameraConnect();

        GstElement *getElement();

    private:
        GstElement *src;
        bool addedToBin;
};

#endif //HAILMARY_GST_ELEMENTS_H
