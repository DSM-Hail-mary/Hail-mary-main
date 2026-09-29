#ifndef HAILMARY_GST_ELEMENTS_H
#define HAILMARY_GST_ELEMENTS_H

#include <gst/gst.h>
#include "main_pipeline.h"

class CameraElement {
    public:
        CameraElement();
        ~CameraElement();

        static bool cameraInit();
        bool cameraConnect();

        GstElement *getElement();

    private:
        GstElement *src;
};

#endif //HAILMARY_GST_ELEMENTS_H
